#include "uf2.h"

#include "sam.h"

#if defined(SAMD21) || defined(SAML21)
#define BOOTLOADER_K 8
#endif
#ifdef SAMD51
#define BOOTLOADER_K 16
#endif

extern const uint8_t bootloader[];
extern const uint16_t bootloader_crcs[];

uint8_t pageBuf[FLASH_ROW_SIZE];

#if defined(SAMD21) || defined(SAML21)
#define NVM_FUSE_ADDR NVMCTRL_AUX0_ADDRESS
#define exec_cmd(cmd)                                                          \
    do {                                                                       \
        NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;                            \
        NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER / 2;                        \
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | cmd;                    \
        while (NVMCTRL->INTFLAG.bit.READY == 0) {}                             \
    } while (0)
#endif
#ifdef SAMD51
#define NVM_FUSE_ADDR NVMCTRL_FUSES_BOOTPROT_ADDR
#define exec_cmd(cmd)                                                          \
    do {                                                                       \
        NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER;                        \
        NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | cmd;                    \
        while (NVMCTRL->STATUS.bit.READY == 0) {}                              \
    } while (0)
#endif

void setBootProt(int v) {
    uint32_t fuses[2];

    #if defined(SAMD21) || defined(SAML21)
    while (!(NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY)) {}
    #endif
    #ifdef SAMD51
    while (NVMCTRL->STATUS.bit.READY == 0) {}
    #endif

    fuses[0] = *((uint32_t *)NVM_FUSE_ADDR);
    fuses[1] = *(((uint32_t *)NVM_FUSE_ADDR) + 1);

    bool repair_fuses = false;
    // Check for damaged fuses. If the NVM user page was accidentally erased, there
    // will be 1's in wrong places. This would enable the watchdog timer and cause other
    // problems. So check for all ones outside of the SAMD21/51 BOOTPROT fields, or all ones
    // in fuses[1]. If it appears the fuses page was erased, replace fuses with reasonable values.
    if ((fuses[0] & 0x03fffff0) == 0x03fffff0 || fuses[1] == 0xffffffff) {
        repair_fuses = true;

        // These canonical fuse values taken from working Adafruit SAMD21 and SAMD51 boards.
        #if defined(SAMD21) || defined(SAML21)
        fuses[0] = 0xD8E0C7FA;
        fuses[1] = 0xFFFFFC5D;
        #endif
        #ifdef SAMD51
        fuses[0] = 0xF69A9239;
        fuses[1] = 0xAEECFF80;
        #endif
    }

    uint32_t bootprot = (fuses[0] & NVMCTRL_FUSES_BOOTPROT_Msk) >> NVMCTRL_FUSES_BOOTPROT_Pos;

    logval("repair_fuses", repair_fuses);
    logval("fuse0", fuses[0]);
    logval("fuse1", fuses[1]);
    logval("bootprot", bootprot);
    logval("needed", v);

    // Don't write if nothing will be changed.
    if (bootprot == v && !repair_fuses) {
        return;
    }

    fuses[0] = (fuses[0] & ~NVMCTRL_FUSES_BOOTPROT_Msk) | (v << NVMCTRL_FUSES_BOOTPROT_Pos);

    #if defined(SAMD21) || defined(SAML21)
    NVMCTRL->CTRLB.reg = NVMCTRL->CTRLB.reg | NVMCTRL_CTRLB_CACHEDIS | NVMCTRL_CTRLB_MANW;

    exec_cmd(NVMCTRL_CTRLA_CMD_EAR);
    exec_cmd(NVMCTRL_CTRLA_CMD_PBC);
    #endif
    #ifdef SAMD51
    NVMCTRL->CTRLA.bit.WMODE = NVMCTRL_CTRLA_WMODE_MAN;

    exec_cmd(NVMCTRL_CTRLB_CMD_EP);
    exec_cmd(NVMCTRL_CTRLB_CMD_PBC);
    #endif

    *((uint32_t *)NVM_FUSE_ADDR) = fuses[0];
    *(((uint32_t *)NVM_FUSE_ADDR) + 1) = fuses[1];

    #if defined(SAMD21) || defined(SAML21)
    exec_cmd(NVMCTRL_CTRLA_CMD_WAP);
    #endif
    #ifdef SAMD51
    exec_cmd(NVMCTRL_CTRLB_CMD_WQW);
    #endif

    resetIntoApp();
}

int main(void) {
    led_init();

    logmsg("Start");

    assert((8 << NVMCTRL->PARAM.bit.PSZ) == FLASH_PAGE_SIZE);
    // assert(FLASH_PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP == FLASH_SIZE);

    /* We have determined we should stay in the monitor. */
    /* System initialization */
    system_init();
    __disable_irq();
    __DMB();

    logmsg("Before main loop");

    #if defined(SAMD21) || defined(SAML21)
    // Disable BOOTPROT while updating bootloader.
    setBootProt(7); // 0k - See "Table 22-2 Boot Loader Size" in datasheet.
    #endif
    #ifdef SAMD51
    // We only need to set the BOOTPROT once on the SAMD51. For updates, we can
    // temporarily turn the protection off instead.
    // setBootProt() will only write BOOTPROT if it is not already correct.
    // setBootProt() will also fix the fuse values if they appear to be all ones.
    setBootProt(13); // 16k. See "Table 25-10 Boot Loader Size" in datasheet.
    exec_cmd(NVMCTRL_CTRLB_CMD_SBPDIS);
    NVMCTRL->CTRLA.bit.CACHEDIS0 = true;
    NVMCTRL->CTRLA.bit.CACHEDIS1 = true;
    #endif

    int i;

#if defined(SAMD21) || defined(SAML21)
    const uint8_t *ptr = bootloader;
    for (i = 0; i < BOOTLOADER_K; ++i) {
        int crc = 0;
        for (int j = 0; j < 1024; ++j) {
            crc = add_crc(*ptr++, crc);
        }
        if (bootloader_crcs[i] != crc) {
            logmsg("Invalid checksum. Aborting.");
            panic(1);
        }
    }
#endif

    for (i = 0; i < BOOTLOADER_K * 1024; i += FLASH_ROW_SIZE) {
        memcpy(pageBuf, &bootloader[i], FLASH_ROW_SIZE);
        flash_write_row((void *)i, (void *)pageBuf);
    }

    logmsg("Update successful!");

    // re-base int vector back to bootloader, so that the flash erase below doesn't write over the
    // vectors
    SCB->VTOR = 0;

    // Write zeros to the stack location and reset handler location so the
    // bootloader doesn't run us a second time. We don't need to erase to write
    // zeros. The remainder of the write unit will be set to 1s which should
    // preserve the existing values but its not critical.
    uint32_t zeros[2] = {0, 0};
    flash_write_words((void *)(BOOTLOADER_K * 1024), zeros, 2);

    for (i = 0; i < 20; ++i) {
        LED_MSC_TGL();
        delay(1000);
    }

    LED_MSC_OFF();

    #if defined(SAMD21) || defined(SAML21)
    // Re-enable BOOTPROT
    setBootProt(2); // 8k
    #endif
    // For the SAMD51, the boot protection will automatically be re-enabled on
    // reset.

    resetIntoBootloader();

    while (1) {
    }
}
