/*
 * ARM Versatile Platform/Application Baseboard System emulation.
 *
 * Copyright (c) 2005-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL.
 */

// #include "sysbus.h"
// #include "arm-misc.h"
// #include "devices.h"
// #include "net.h"
// #include "sysemu.h"
// #include "pci.h"
// #include "i2c.h"
// #include "boards.h"
// #include "blockdev.h"
// #include "flash.h"
// #include "elf.h"
// #include "qjson.h"
// #include "qlist.h"
// #include "exec-memory.h"
#include "sysbus.h"
#include "devices.h"
#include "boards.h"
#include "qjson.h"
#include "qobject.h"
#include "qint.h"
#include "qdict.h"
#include "exec-memory.h"

#ifdef TARGET_ARM
#include <target-arm/helper.h>
#include <target-arm/cpu.h>
#endif

/* Board init.  */

/* The AB and PB boards both use the same core, just with different
   peripherals and expansion busses.  For now we emulate a subset of the
   PB peripherals and just change the board ID.  */

// static struct arm_boot_info versatile_binfo;

static QDict * load_configuration(const char * filename)
{
    int file = open(filename, O_RDONLY);
    off_t filesize = lseek(file, 0, SEEK_END);
    char * filedata = NULL;
    ssize_t err;
    QObject * obj;

    lseek(file, 0, SEEK_SET);

    filedata = g_malloc(filesize + 1);
    memset(filedata, 0, filesize + 1);

    if (!filedata)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    err = read(file, filedata, filesize);

    if (err != filesize)
    {
        fprintf(stderr, "Reading configuration file failed\n");
        exit(1);
    }

    close(file);

    obj = qobject_from_jsonv(filedata, NULL);

    if (!obj || qobject_type(obj) != QTYPE_QDICT)
    {
        fprintf(stderr, "Error parsing JSON configuration file\n");
        exit(1);
    }

    g_free(filedata);

    return qobject_to_qdict(obj);
}

/**
 * Return how many characters of the string are part of the directory path, or 0 if the file is specified without directory.
 */
static int get_dirname_len(const char * filename)
{
    int i;

    for (i = strlen(filename) - 1; i >= 0; i--)
    {
        //FIXME: This is only Linux-compatible ...
        if (filename[i] == '/')
        {
            return i + 1;
        }
    }

    return 0;
}

static int is_absolute_path(const char * filename)
{
    return filename[0] == '/';
}

static void dummy_interrupt(void * opaque, int irq, int level)
{
}
#ifdef TARGET_ARM

// HACK HACK HACK cut'paste from helper.c
/* Map CPU modes onto saved register banks.  */
static inline int bank_number(CPUArchState *env, int mode)
{
    switch (mode) {
    case ARM_CPU_MODE_USR:
    case ARM_CPU_MODE_SYS:
        return 0;
    case ARM_CPU_MODE_SVC:
        return 1;
    case ARM_CPU_MODE_ABT:
        return 2;
    case ARM_CPU_MODE_UND:
        return 3;
    case ARM_CPU_MODE_IRQ:
        return 4;
    case ARM_CPU_MODE_FIQ:
        return 5;
    }
    cpu_abort(env, "Bad mode %x\n", mode);
    return -1;
}
#endif

/**
 * Synchronize registers initial state from JSON
 */
static void machine_init_state(QDict * conf, CPUArchState * cpu) {
	g_assert(qdict_haskey(conf, "init_state"));

	QListEntry * entry;
	g_assert(qobject_type(qdict_get(conf, "init_state")) ==  QTYPE_QLIST);
	QList * registers = qobject_to_qlist(qdict_get(conf, "init_state"));
	g_assert(registers);

	QLIST_FOREACH_ENTRY(registers, entry) {
		QDict * reg;
		QDictEntry * elm;
		target_ulong regvalue;

		g_assert(qobject_type(entry->value) == QTYPE_QDICT);
		reg = qobject_to_qdict(entry->value);


#ifdef TARGET_ARM

		// First search and set CPSR, then we are properly set, don't change mode anymore
		for (elm = (QDictEntry *) qdict_first(reg); elm != NULL; elm = (QDictEntry *) qdict_next(reg, elm)) {
		  const char * regname;
		  g_assert(qobject_type(qdict_entry_value(elm)) == QTYPE_QINT);
		  regname = strdup(qdict_entry_key(elm));
		  regvalue = qint_get_int(qobject_to_qint(qdict_entry_value(elm)));
		  char * separator = strchr(regname, '_');
		  if (separator == NULL) {
		    if (separator ==NULL && strcmp(regname, "cpsr") == 0)
		      cpsr_write(cpu, regvalue, 0xFFFFFFFF);
		  }
		  free((void*) regname);
		}

#endif
		  for (elm = (QDictEntry *) qdict_first(reg); elm != NULL; elm = (QDictEntry *) qdict_next(reg, elm)) {
			const char * regname;
			g_assert(qobject_type(qdict_entry_value(elm)) == QTYPE_QINT);
			regname = strdup(qdict_entry_key(elm));
			regvalue = qint_get_int(qobject_to_qint(qdict_entry_value(elm)));

#ifdef TARGET_ARM
			// TODO: this is very fragile, rework it in a sort-of key filtering
			char * separator = strchr(regname, '_');
			if (separator == NULL) {
				//base registers
				if (regname[0] == 'r') {
					WR_cpu(cpu, regs[atoi(&regname[1])], regvalue);
				}
				else if (strcmp(regname, "pc") == 0) {
					((CPUARMState *) cpu)->regs[15] = regvalue;
					// FIXME: something is wrong with offsets in assertion in
					// s2e_write_register_concrete codepath
					// WR_cpu(cpu, regs[15], regvalue);
				}
			} else {
				// banked registers
				int mode=0;
				*separator = '\0';
				if (strcmp(separator+1, "fiq") == 0) {
					mode = ARM_CPU_MODE_FIQ;
					if (regname[0] == 'r') {
						WR_cpu(cpu, fiq_regs[atoi(&regname[1])-8], regvalue);
					}
				}
				else if (strcmp(separator+1, "irq") == 0) {
					mode = ARM_CPU_MODE_IRQ;
				}
				else if (strcmp(separator+1, "und") == 0) {
					mode = ARM_CPU_MODE_UND;
				}
				else if (strcmp(separator+1, "usr") == 0) {
					mode = ARM_CPU_MODE_USR;
				}
				else if (strcmp(separator+1, "svc") == 0) {
					mode = ARM_CPU_MODE_SVC;
				}
				else if (strcmp(separator+1, "abt") == 0) {
					mode = ARM_CPU_MODE_ABT;
				}

				if (mode != 0) {
					if (strcmp(regname, "sp") == 0) {
					  printf("cpu->uncached_cpsr = 0x%x, sp=%x\n",cpu->uncached_cpsr,regvalue);

					  if (mode == ARM_CPU_MODE_USR) {
					    cpu->regs[13] = regvalue;
					  }
					  cpu->banked_r13[bank_number(cpu, mode)] = regvalue;
					}
					else if (strcmp(regname, "lr") == 0) {

					  if (mode == ARM_CPU_MODE_USR) {
					    cpu->regs[14] = regvalue;
					  }
					  cpu->banked_r14[bank_number(cpu, mode)] = regvalue;

					}
					else if (strcmp(regname, "spsr") == 0) {
					  cpu->banked_spsr[bank_number(cpu, mode)] = regvalue;
					}

				}
			}
#elif TARGET_I386
			g_assert(false && "Not implemented for Intel");
#endif
			free((void*) regname);
		}
	}
}


static void board_init(ram_addr_t ram_size,
                     const char *boot_device,
                     const char *kernel_filename, const char *kernel_cmdline,
                     const char *initrd_filename, const char *cpu_model)
{
    CPUArchState * cpu;
    QDict * conf = NULL;
    uint64_t entry_address=0;

    //Load configuration file
    if (kernel_filename)
    {
        conf = load_configuration(kernel_filename);
    }
    else
    {
        conf = qdict_new();
    }

    //Configure CPU
    if (qdict_haskey(conf, "cpu_model"))
    {
        cpu_model = qdict_get_str(conf, "cpu_model");
        g_assert(cpu_model);
    }

#ifdef TARGET_ARM
    if (!cpu_model) cpu_model = "arm926";
#endif

    printf("Configurable: Adding processor %s\n", cpu_model);

    cpu = cpu_init(cpu_model);

    if (!cpu)
    {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

#ifdef CONFIG_S2E
    s2e_register_cpu(g_s2e, g_s2e_state, cpu);
#endif

    //Configure memory
    if (qdict_haskey(conf, "memory_map"))
    {
        QListEntry * entry;
        QList * memory_map = qobject_to_qlist(qdict_get(conf, "memory_map"));
        g_assert(memory_map);
        MemoryRegion *sysmem = get_system_memory();

        QLIST_FOREACH_ENTRY(memory_map, entry)
        {
            QDict * mapping;
            QList * addresses;
            QListEntry * address_entry;
            uint64_t size;
            uint64_t data_size;
            char * data = NULL;
            const char * name;
            MemoryRegion * ram;
            int is_rom = FALSE; //TODO: Currently ignored, only RAM is used
            uint64_t address;
            int is_first_mapping = TRUE;
            int alias_num = 0;
 //           void * ram_ptr;

            g_assert(qobject_type(entry->value) == QTYPE_QDICT);
            mapping = qobject_to_qdict(entry->value);

            g_assert(qdict_haskey(mapping, "map") && qobject_type(qdict_get(mapping, "map")) == QTYPE_QLIST && !qlist_empty(qobject_to_qlist(qdict_get(mapping, "map"))));
            g_assert(qdict_haskey(mapping, "name") && qobject_type(qdict_get(mapping, "name")) == QTYPE_QSTRING);
            g_assert(!qdict_haskey(mapping, "is_rom") || qobject_type(qdict_get(mapping, "is_rom")) == QTYPE_QBOOL);
            g_assert(qdict_haskey(mapping, "size") && qobject_type(qdict_get(mapping, "size")) == QTYPE_QINT);
            g_assert((qdict_get_int(mapping, "size") & ((1 << 12) - 1)) == 0); //Somewhere deep in QEMU there seems to be a requirement that memory regions have starting addresses with the lowest 12 bit clear

            addresses = qobject_to_qlist(qdict_get(mapping, "map"));
            name = qdict_get_str(mapping, "name");
            is_rom = qdict_haskey(mapping, "is_rom") && qdict_get_bool(mapping, "is_rom");
            size = qdict_get_int(mapping, "size");

            ram =  g_new(MemoryRegion, 1);
            g_assert(ram);

            //TODO: If is ROM, insert ROM here instead of RAM
            memory_region_init_ram(ram, name, size);

            QLIST_FOREACH_ENTRY(addresses, address_entry)
            {
                QDict * address_dict;
                g_assert(qobject_type(address_entry->value) == QTYPE_QDICT);
                address_dict = qobject_to_qdict(address_entry->value);
                g_assert(qdict_haskey(address_dict, "address") && qobject_type(qdict_get(address_dict, "address")) == QTYPE_QINT);

                address = qdict_get_int(address_dict, "address");

                if (is_first_mapping)
                {
                    printf("Configurable: Adding memory region %s (size: 0x%lx) at address 0x%lx\n", name, size, address);
                    memory_region_add_subregion(sysmem, address, ram);
                    is_first_mapping = FALSE;

#ifdef CONFIG_S2E
            s2e_register_ram(g_s2e, g_s2e_state,
                  address, size,
                  (uint64_t) memory_region_get_ram_ptr(ram), 0, 0, name);
#endif
                }
                else
                {
                    MemoryRegion * ram_alias =  g_new(MemoryRegion, 1);
                    g_assert(ram_alias);
                    char alias_name[60];

                    snprintf(alias_name, sizeof(alias_name), "%s.alias_%d", name, alias_num);

                    printf("Configurable: Adding memory region %s (size: 0x%lx) at address 0x%lx\n", alias_name, size, address);
                    memory_region_init_alias(ram_alias, alias_name, ram, 0, size);
                    memory_region_add_subregion(sysmem, address, ram_alias);

                    /* Adding an alias memory region to S2E via s2e_register_ram causes a segfault, so don't do it. */

//Apparently you only add ram once to S2E - otherwise segfault :/
// #ifdef CONFIG_S2E
//             s2e_register_ram(g_s2e, g_s2e_state,
//                   address, size,
//                   (uint64_t) memory_region_get_ram_ptr(ram_alias), 0, 0, alias_name);
// #endif
                }

                alias_num += 1;
            }

            if (qdict_haskey(mapping, "file"))
            {
                int file;
                const char * filename;
                int dirname_len = get_dirname_len(kernel_filename);
                ssize_t err;

                g_assert(qobject_type(qdict_get(mapping, "file")) == QTYPE_QSTRING);
                filename = qdict_get_str(mapping, "file");

                if (!is_absolute_path(filename))
                {
                    char * relative_filename = g_malloc0(dirname_len + strlen(filename) + 1);
                    g_assert(relative_filename);
                    strncpy(relative_filename, kernel_filename, dirname_len);
                    strcat(relative_filename, filename);

                    file = open(relative_filename, O_RDONLY | O_BINARY);
                    g_free(relative_filename);
                }
                else
                {
                    file = open(filename, O_RDONLY | O_BINARY);
                }

                data_size = lseek(file, 0, SEEK_END);
                lseek(file, 0, SEEK_SET);

                g_assert(data_size <= size); //Size of data to put into a RAM region needs to fit in the RAM region

                data = g_malloc(data_size);
                g_assert(data);

                err = read(file, data, data_size);
                g_assert(err == data_size);

                close(file);

                //And copy the data to the memory, if it is initialized
                printf("Configurable: Copying 0x%lx byte of data from file %s to address 0x%lx\n", data_size, filename, address);
//                ram_ptr = qemu_get_ram_ptr(memory_region_get_ram_addr(ram));
//                memcpy(ram_ptr, data, data_size);
//                qemu_put_ram_ptr(ram_ptr);
                cpu_physical_memory_write_rom(address, (uint8_t *) data, data_size);
                g_free(data);
            }

        }
    }

    /*
     * The devices stuff is just considered a hack, I want to replace everything here with a device tree parser as soon as I have the time ...
     */
     if (qdict_haskey(conf, "devices"))
    {
        QListEntry * entry;
        QList * devices = qobject_to_qlist(qdict_get(conf, "devices"));
        g_assert(devices);

        QLIST_FOREACH_ENTRY(devices, entry)
        {
            QDict * device;

            const char * qemu_name;
            const char * bus;
            uint64_t address;
            qemu_irq* irq;

            g_assert(qobject_type(entry->value) == QTYPE_QDICT);
            device = qobject_to_qdict(entry->value);

            g_assert(qdict_haskey(device, "address") && qobject_type(qdict_get(device, "address")) == QTYPE_QINT);
            g_assert(qdict_haskey(device, "qemu_name") && qobject_type(qdict_get(device, "qemu_name")) == QTYPE_QSTRING);
            g_assert(qdict_haskey(device, "bus") && qobject_type(qdict_get(device, "bus")) == QTYPE_QSTRING);

            bus = qdict_get_str(device, "bus");
            qemu_name = qdict_get_str(device, "qemu_name");
            address = qdict_get_int(device, "address");



            if (strcmp(bus, "sysbus") == 0)
            {
                //For now only dummy interrupts ...
                irq = qemu_allocate_irqs(dummy_interrupt, NULL, 1);
                sysbus_create_simple(qemu_name, address, *irq);
            }
            else
            {
                g_assert(0); //Right now only sysbus devices are supported ...
            }
        }
    }

    //sysbus_create_simple("xilinx,uartlite", UARTLITE_BASEADDR, irq[3]);

    // TODO: merge entry_address with init_state.pc handling
    // Set init state
    g_assert((qdict_haskey(conf, "entry_address") || qdict_haskey(conf, "init_state")));
    if (qdict_haskey(conf, "entry_address")) {
	g_assert(qobject_type(qdict_get(conf, "entry_address")) == QTYPE_QINT);
	entry_address = qdict_get_int(conf, "entry_address");
        // Just set the entry address
#ifdef TARGET_ARM
        ((CPUARMState *) cpu)->thumb = (entry_address & 1) != 0 ? 1 : 0;
        ((CPUARMState *) cpu)->regs[15] = entry_address & (~1);
#elif TARGET_I386
        ((CPUX86State *) cpu)->eip = entry_address;
#endif
    }
    if (qdict_haskey(conf, "init_state")) {
	// Optionally set the whole initial state
	printf("Configurable: Setting initial state from JSON.\n");
	machine_init_state(conf, cpu);
#ifdef TARGET_ARM
	entry_address = ((CPUARMState *) cpu)->regs[15] ;
#elif TARGET_I386
        entry_address = ((CPUX86State *) cpu)->eip;
#endif
    }

    printf("Configurable: Ready to start at 0x%lx.\n", (unsigned long) entry_address);
}

static QEMUMachine configurable_machine = {
    .name = "configurable",
    .desc = "Machine that can be configured to be whatever you want",
    .init = board_init,
};

static void configurable_machine_init(void)
{
    qemu_register_machine(&configurable_machine);
}

machine_init(configurable_machine_init);
