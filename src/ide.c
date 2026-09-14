#include "include/ide.h"

channel_t channels[2]; // 0: primary channel, 1: secondary channel
ide_device_t ide_devices[4]; // 0: primary master, 1: primary slave, 2: secondary master, 3: secondary slave
int initialized = 0; // flag to indicate if ide has been initialized, should be set to 1 at the end of ide_initialize

uint8_t ide_buf[2048] = {0};
volatile unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


int ide_cmd_translation(uint8_t lba_mode, uint8_t dma, uint8_t direction) {
    int cmd = 0;
    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    return cmd;
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

static inline void ide_400ns(int ch) {
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
    ide_read(ch, ATA_REG_ALTSTATUS);
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

uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, uint16_t selector, uint32_t edi){
    uint32_t cmd;
    uint8_t lba_mode,dma;
    uint8_t lba_io[6];
    uint8_t channel = ide_devices[drive].Channel;
    uint8_t slavebit = ide_devices[drive].Drive;
    uint16_t bus = channels[channel].base;
    uint32_t words = 256;
    uint16_t cyl, i;
    uint8_t head, sect;
    uint8_t err;
    if (lba > 0x0FFFFFFF) { // lba48
        lba_mode = 2;
        lba_io[0] = (lba & 0xFF);
        lba_io[1] = (lba >> 8) & 0xFF;
        lba_io[2] = (lba >> 16) & 0xFF;
        lba_io[3] = (lba >> 24) & 0x0FF;
        lba_io[4] = 0;  
        lba_io[5] = 0;
        head = 0;      
    } else if (ide_devices[drive].Capabilities & 0x200) { // lba28
        lba_mode = 1;
        lba_io[0] = (lba & 0xFF);
        lba_io[1] = (lba >> 8) & 0xFF;
        lba_io[2] = (lba >> 16) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba >> 24) & 0xF;
    } else { //chs
        lba_mode = 0;
        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl & 0xFF);
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba + 1  - sect) % (16 * 63) / (63);
    }
    dma = 0; //PIO fro now
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);
    if (lba_mode == 0) {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head);
    } else {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
    }
    ide_400ns(channel);
    if (lba_mode == 2) {
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_400ns(channel);
        ide_write(channel, ATA_REG_LBA3, lba_io[3]);
        ide_400ns(channel);
        ide_write(channel, ATA_REG_LBA4, lba_io[4]);
        ide_400ns(channel);
        ide_write(channel, ATA_REG_LBA5, lba_io[5]);
        ide_400ns(channel);
    }
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_400ns(channel);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_400ns(channel);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);
    ide_400ns(channel);

    cmd = ide_cmd_translation(lba_mode, dma, direction);
    ide_write(channel, ATA_REG_COMMAND, cmd);
    if (dma){
        return -1; //DMA not supported yet
    } else {
        if (direction == 0) { // Read
            for (int i = 0; i < numsects; i++) {
                if ((err = ide_polling(channel, 1)))
                    return err;
                __asm__ volatile("pushw %ds");
                __asm__ volatile("mov %%ax, %%ds": : "a" (selector));
                __asm__ volatile("rep insw": : "c" (words), "d" (bus), "D" (edi));
                __asm__ volatile("popw %ds");
                edi += words * 2;
            }
        } else { // write
            for (int i = 0; i < numsects; i++) {
                if ((err = ide_polling(channel, 0)))
                    return err;
                __asm__ volatile("pushw %ds");
                __asm__ volatile("mov %%ax, %%ds": : "a" (selector));
                __asm__ volatile("rep outsw": : "c" (words), "d" (bus), "S" (edi));
                __asm__ volatile("popw %ds");
                edi += words * 2;
            }
            ide_write(channel, ATA_REG_COMMAND, (char []) {   ATA_CMD_CACHE_FLUSH,
                        ATA_CMD_CACHE_FLUSH,
                        ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
            ide_polling(channel, 0); // Polling.
        }

    }
    return 0;
}

// Initialize the IDE controllers and detect connected drives
// @param BAR0 Base Address Register 0
// @param BAR1 Base Address Register 1
// @param BAR2 Base Address Register 2
// @param BAR3 Base Address Register 3
// @param BAR4 Base Address Register 4
void ide_initialize(uint32_t BAR0, uint32_t BAR1, uint32_t BAR2, uint32_t BAR3, uint32_t BAR4) {
    if (initialized) {
        return; // already initialized, do nothing
    }
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
    initialized = 1;
}
// Read sectors from the specified drive
// @param drive Drive number (0-3)
// @param lba Logical Block Addressing sector number
// @param numsects Number of sectors to read
// @param selector Code segment selector
// @param edi Destination address to store the read data
// @return 0 on success, error code on failure
uint8_t ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t numsects, uint16_t es, uint32_t edi) {
    if (drive == 0) { // protect primary master (usually system disk)
        return 4;
    }
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        return 1;
    else if (((lba + numsects) > ide_devices[drive].Size) && ide_devices[drive].Type == IDE_ATA)
        return 2;
    else {
        uint8_t err = 0;
        if (ide_devices[drive].Type == IDE_ATA){
            err = ide_ata_access(ATA_READ, drive, lba, numsects, es, edi);
        } else if (ide_devices[drive].Type == IDE_ATAPI){
            return 3; //ATAPI not supported yet
        }
        return err;
    }
}
// Write sectors to the specified drive
// @param drive Drive number (0-3)
// @param lba Logical Block Addressing sector number
// @param numsects Number of sectors to write
// @param selector Code segment selector
// @param edi Source address of the data to write
// @return 0 on success, error code on failure
uint8_t ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t numsects, uint16_t es, uint32_t edi) {
    if (drive == 0) { // protect primary master (usually system disk)
        return 4;
    }
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        return 1;
    else if (((lba + numsects) > ide_devices[drive].Size) && ide_devices[drive].Type == IDE_ATA)
        return 2;
    else {
        uint8_t err = 0;
        if (ide_devices[drive].Type == IDE_ATA){
            err = ide_ata_access(ATA_WRITE, drive, lba, numsects, es, edi);
        } else if (ide_devices[drive].Type == IDE_ATAPI){
            return 3; //ATAPI not supported yet
        }
        return err;
    }
}

