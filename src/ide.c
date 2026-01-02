#include "include/ide.h"

channel_t channels[2]; // 0: primary channel, 1: secondary channel
ide_device_t ide_devices[4]; // 0: primary master, 1: primary slave, 2: secondary master, 3: secondary slave

uint8_t ide_buf[2048] = {0};
volatile unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static inline void ide_400ns(int ch) {
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
}

void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        outb(data,channels[channel].base  + reg - 0x00);
    else if (reg < 0x0C)
        outb(data,channels[channel].base  + reg - 0x06);
    else if (reg < 0x0E)
        outb(data, channels[channel].ctrl  + reg - 0x0A);
    else if (reg < 0x16)
        outb(data, channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}
uint8_t ide_read(uint8_t channel, uint8_t reg){
    unsigned char result;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        result = inb(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(channels[channel].base  + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(channels[channel].ctrl  + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}
uint8_t ide_polling(uint8_t channel, uint32_t advanced_check) {
    // (I) Delay 400 nanosecond for BSY to be set:
   // -------------------------------------------------
   for(int i = 0; i < 4; i++)
      ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.

   // (II) Wait for BSY to be cleared:
   // -------------------------------------------------
   while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
      ; // Wait for BSY to be zero.

   if (advanced_check) {
      unsigned char state = ide_read(channel, ATA_REG_STATUS); // Read Status Register.

      // (III) Check For Errors:
      // -------------------------------------------------
      if (state & ATA_SR_ERR)
         return 2; // Error.

      // (IV) Check If Device fault:
      // -------------------------------------------------
      if (state & ATA_SR_DF)
         return 1; // Device Fault.

      // (V) Check DRQ:
      // -------------------------------------------------
      // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
      if ((state & ATA_SR_DRQ) == 0)
         return 3; // DRQ should be set

   }

   return 0; // No Error.
}
void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t buffer,
                     uint32_t quads) {
   /* WARNING: This code contains a serious bug. The inline assembly trashes ES and
    *           ESP for all of the code the compiler generates between the inline
    *           assembly blocks.
    */
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
   __asm__ volatile("pushw %es; movw %ds, %ax; movw %ax, %es");
   if (reg < 0x08)
      insl(channels[channel].base  + reg - 0x00, buffer, quads);
   else if (reg < 0x0C)
      insl(channels[channel].base  + reg - 0x06, buffer, quads);
   else if (reg < 0x0E)
      insl(channels[channel].ctrl  + reg - 0x0A, buffer, quads);
   else if (reg < 0x16)
      insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
   __asm__ volatile("popw %es;");
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

uint8_t ide_print_error(uint32_t drive, uint8_t err) {
    if (err == 0)
        return err;

    terminal_writestring("IDE:");
    if (err == 1) {terminal_writestring("- Device Fault\n     "); err = 19;}
    else if (err == 2) {
        unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
        if (st & ATA_ER_AMNF)   {terminal_writestring("- No Address Mark Found\n     ");   err = 7;}
        if (st & ATA_ER_TK0NF)   {terminal_writestring("- No Media or Media Error\n     ");   err = 3;}
        if (st & ATA_ER_ABRT)   {terminal_writestring("- Command Aborted\n     ");      err = 20;}
        if (st & ATA_ER_MCR)   {terminal_writestring("- No Media or Media Error\n     ");   err = 3;}
        if (st & ATA_ER_IDNF)   {terminal_writestring("- ID mark not Found\n     ");      err = 21;}
        if (st & ATA_ER_MC)   {terminal_writestring("- No Media or Media Error\n     ");   err = 3;}
        if (st & ATA_ER_UNC)   {terminal_writestring("- Uncorrectable Data Error\n     ");   err = 22;}
        if (st & ATA_ER_BBK)   {terminal_writestring("- Bad Sectors\n     ");       err = 13;}
    } else  if (err == 3)           {terminal_writestring("- Reads Nothing\n     "); err = 23;}
    else  if (err == 4)  {terminal_writestring("- Write Protected\n     "); err = 8;}


   return err;
}
void ide_initialize(uint32_t BAR0, uint32_t BAR1, uint32_t BAR2, uint32_t BAR3, uint32_t BAR4) {
    int i,j, k, count = 0;
    char int_buf[10];

    // 1- Detect I/O Ports which interface IDE Controller:
    channels[ATA_PRIMARY  ].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
    channels[ATA_PRIMARY  ].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
    channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
    channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
    channels[ATA_PRIMARY  ].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
    channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE
    // 2- Disable IRQs:
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_400ns(ATA_PRIMARY);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);
    ide_400ns(ATA_SECONDARY);
    // 3- Detect ATA-ATAPI Devices:
    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++) {

            unsigned char err = 0, type = IDE_ATA, status;
            ide_devices[count].Reserved = 0; // Assuming that no drive here.

            // (I) Select Drive:
            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
            ide_400ns(i);

            

            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            // it is based on System Timer Device Driver.

            // (III) Polling:
            if (ide_read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.

            while(1) {
                status = ide_read(i, ATA_REG_STATUS);
                //itoa(int_buf, 'x', status);
                //terminal_initialize();
                //terminal_writestring(int_buf);
                if ((status & ATA_SR_ERR)) {err = 1; break;} // If Err, Device is not ATA.
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break; // Everything is right.
            }

            // (IV) Probe for ATAPI Devices:

            if (err != 0) {
                unsigned char cl = ide_read(i, ATA_REG_LBA1);
                unsigned char ch = ide_read(i, ATA_REG_LBA2);

                if (cl == 0x14 && ch == 0xEB)
                type = IDE_ATAPI;
                else if (cl == 0x69 && ch == 0x96)
                type = IDE_ATAPI;
                else
                continue; // Unknown Type (may not be a device).

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                ide_400ns(i);
            }

            // (V) Read Identification Space of the Device:
            ide_read_buffer(i, ATA_REG_DATA, (uint32_t) ide_buf, 128);

            // (VI) Read Device Parameters:
            ide_devices[count].Reserved     = 1;
            ide_devices[count].Type         = type;
            ide_devices[count].Channel      = i;
            ide_devices[count].Drive        = j;
            ide_devices[count].Signature    = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets  = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size:
            if (ide_devices[count].CommandSets & (1 << 26))
                // Device uses 48-Bit Addressing:
                ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else
                // Device uses CHS or 28-bit Addressing:
                ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));

            // (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
            for(k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].Model[40] = 0; // Terminate String.

            count++;
        }
    // 4- Print Summary:
    for (i = 0; i < 4; i++){
        if (ide_devices[i].Reserved == 1) {
            terminal_writestring("Found ");
            itoa(int_buf, 'd', ide_devices[i].Type);
            terminal_writestring((ide_devices[i].Type == IDE_ATA) ? "ATA Drive " : "ATAPI Drive ");
            itoa(int_buf, 'd', ide_devices[i].Size/2);
            terminal_writestring(int_buf);
            terminal_writestring("bytes - ");
            terminal_writestring((char*)ide_devices[i].Model);
            terminal_writestring("\n");
        }
    }
}

