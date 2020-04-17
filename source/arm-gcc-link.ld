OUTPUT_FORMAT ("elf32-littlearm")

/*memory map*/
MEMORY
{
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 0x00100000
	ram (rw) : ORIGIN = 0x20000000, LENGTH = 0x00020000
	ccm (rw) : ORIGIN = 0x10000000, LENGTH = 0x00010000
}

_start_of_ram = ORIGIN(ram);
_end_of_ram   = ORIGIN(ram)+LENGTH(ram);

_start_of_ccm = ORIGIN(ccm);
_end_of_ccm   = ORIGIN(ccm)+LENGTH(ccm);

SECTIONS
{
	.text :
	{
		KEEP(*(.isr_vector))
		*(.text*)
		
		. = ALIGN(4);
		
		KEEP(*(.init))
		KEEP(*(.fini))
		
		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)
		
		/* .dtors */
		*crtbegin.o(.dtors)
		*crtbegin?.o(.dtors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
		*(SORT(.dtors.*))
		*(.dtors)
		
		. = ALIGN(4);
		
		*(.rodata*)
		
		KEEP(*(.eh_fram e*))
	} > rom 
	
	.ARM.extab : 
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > rom 
	
	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > rom 
	__exidx_end = .;
	
	. = ALIGN(4);
	__etext = .;
	
	/*used in the boot code*/
	_start_of_initdata = __etext;
	
	.data : AT (__etext)
	{
		. = ALIGN(4);
		
		/*used in the boot code*/
		_start_of_data = .;
		
		*(vtable)
		. = ALIGN(4);
		*(.data*)
		
		. = ALIGN(4);
		/*preinit data*/
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
		
		. = ALIGN(4);
		/*init data*/
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);
		
		. = ALIGN(4);
		/*finit data*/
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);
		
		KEEP(*(.jcr*))
		. = ALIGN(4);

		/*used in the boot code*/
		_end_of_data = .;
	} > ram 
	
	.bss :
	{
		. = ALIGN(4);
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end__ = .;
	} > ram 
	
	.ct_stack (NOLOAD) :
	{
		. = ORIGIN(ccm);
		. = ALIGN(4);
		_bottom_of_stack = .;
		*(.ct_stack .ct_stack.*)
		_top_of_stack = .;
	} > ccm
	
	.ccm (NOLOAD) :
	{
		. = ALIGN(4);
		*(.ccm*)
		. = ALIGN(4);
		__ccm_end__ = .;
	} > ccm
	
	/*check that things fit in RAM*/
	ASSERT(__bss_end__ < ORIGIN(ram)+LENGTH(ram), "region RAM overflowed!")
	
	/*check that things fit in CCM*/
	ASSERT(__ccm_end__ < ORIGIN(ccm)+LENGTH(ccm), "region CCM overflowed!")
}