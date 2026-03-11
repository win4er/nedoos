#include "multiboot.h"

int kmain(/* additional arguments */ unsigned int ebx)
{
	multiboot_info_t *mbinfo = (multiboot_info_t *) ebx;
	unsigned int address_of_module = mbinfo->mods_addr;
}
