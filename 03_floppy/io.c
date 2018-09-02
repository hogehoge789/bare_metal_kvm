#include <stdio.h>
#include <linux/kvm.h>
#include <sys/types.h>
#include "util.h"
#include "serial.h"
#include "fdc.h"

#define RTC_IO_BASE	0x0070
#define RTC_IO_MASK	0xfff0

#define PS2_IO_BASE	0x0090
#define PS2_IO_MASK	0xfff0
#define PS2_IO_SCPA	0x0092

#define DMA2_IO_BASE	0x00c0
#define DMA2_IO_MASK	0xffc0

#define KBC_IO_BASE	0x0060
#define KBC_IO_MASK	0xfff0

#define PARALLEL_PRINTER_IO_BASE	0x0378
#define PARALLEL_PRINTER_IO_MASK	0xfff8
#define PARALLEL_PRINTER2_IO_BASE	0x0278
#define PARALLEL_PRINTER2_IO_MASK	0xfff8

#define UPW48_IO_BASE	0x0200
#define UPW48_IO_MASK	0xff00

#define HDC1_IO_BASE	0x01f0
#define HDC1_IO_MASK	0xfff0
#define HDC2_IO_BASE	0x0170
#define HDC2_IO_MASK	0xfff8

#define PCI_IO_BASE	0x0cf8
#define PCI_IO_MASK	0xfff8
#define PCI_IO_CONFIG_ADDR	0x0cf8
#define PCI_IO_CONFIG_DATA	0x0cfc

#define DEBUG_DUMP_IO_ACCESS

static void dump_io_access_begin(struct kvm_run *run) {
#ifdef DEBUG_DUMP_IO_ACCESS
	unsigned int i;

	fprintf(stderr, "### io: direction=%d, size=%d, port=0x%04x,",
		run->io.direction, run->io.size, run->io.port);
	fprintf(stderr, " count=0x%08x, data=0x",
		run->io.count);
	if (run->io.direction == KVM_EXIT_IO_OUT) {
		for (i = 0; i < run->io.count; i++) {
			switch (run->io.size) {
			case 1:
				fprintf(stderr, "%02x ",
					*(unsigned char *)((unsigned char *)run + run->io.data_offset));
				break;
			case 2:
				fprintf(stderr, "%04x ",
					*(unsigned short *)((unsigned char *)run + run->io.data_offset));
				break;
			case 4:
				fprintf(stderr, "%08x ",
					*(unsigned int *)((unsigned char *)run + run->io.data_offset));
				break;
			}
		}
		fprintf(stderr, "\n");
	}
#endif
}

static void dump_io_access_end(struct kvm_run *run) {
#ifdef DEBUG_DUMP_IO_ACCESS
	if (run->io.direction == KVM_EXIT_IO_IN) {
		switch (run->io.size) {
		case 1:
			fprintf(stderr, "%02x ",
				*(unsigned char *)((unsigned char *)run + run->io.data_offset));
			break;
		case 2:
			fprintf(stderr, "%04x ",
				*(unsigned short *)((unsigned char *)run + run->io.data_offset));
			break;
		case 4:
			fprintf(stderr, "%08x ",
				*(unsigned int *)((unsigned char *)run + run->io.data_offset));
			break;
		}
		fprintf(stderr, "\n");
	}
#endif
}

void io_handle(struct kvm_run *run)
{
	DEBUG_PRINT("io: KVM_EXIT_IO\n");
	DEBUG_PRINT("io: direction=%d, size=%d, port=0x%04x,",
		    run->io.direction, run->io.size, run->io.port);
	DEBUG_PRINT(" count=0x%08x, data_offset=0x%016llx\n",
		    run->io.count, run->io.data_offset);

	dump_io_access_begin(run);

	unsigned long long skip_io = 0;

	if (run->io.port == 0x03f2)
		printf("##### FDC #####\n");

	if ((run->io.port & SERIAL_IO_MASK) == SERIAL_IO_BASE)
		skip_io++;
	else if (run->io.port == SERIAL_IO_TX)
		serial_handle_io(run);
	else if ((run->io.port & RTC_IO_MASK) == RTC_IO_BASE) {
		static unsigned char rtc_index = 0;
		if ((run->io.port == 0x70)
		    && (run->io.direction == KVM_EXIT_IO_OUT)) {
			rtc_index =
				*(unsigned char *)((unsigned char *)run
						   + run->io.data_offset)
				& 0x7f;
		} else if ((run->io.port == 0x71)
			&& (run->io.direction == KVM_EXIT_IO_IN)) {
			switch (rtc_index) {
			case 0x0f:
				*(unsigned char *)((unsigned char *)run
						   + run->io.data_offset) = 0;
				break;
			case 0x34:
				*(unsigned char *)((unsigned char *)run
						   + run->io.data_offset) = 8;
				break;
			}
		} /* else { */
		/* 	printf("##### RTC\n"); */
		/* 	dump_io_access_begin(run); */
		/* } */
	} else if ((run->io.port & PS2_IO_MASK) == PS2_IO_BASE) {
		if ((run->io.port == PS2_IO_SCPA)
		    && (run->io.count == 1) && (run->io.size == 1)) {
			if (run->io.direction == KVM_EXIT_IO_IN)
				*(unsigned char *)((unsigned char *)run + run->io.data_offset) = 0x00;
		} /* else { */
		/* 	dump_io_access_begin(run); */
		/* 	fflush(stdout); */
		/* 	assert(0, "undefined PS2\n"); */
		/* } */
	} else if ((run->io.port & DMA2_IO_MASK) == DMA2_IO_BASE)
		skip_io++;
	else if ((run->io.port & KBC_IO_MASK) == KBC_IO_BASE)
		skip_io++;
	else if (((run->io.port & PARALLEL_PRINTER_IO_MASK) == PARALLEL_PRINTER_IO_BASE)
		 || ((run->io.port & PARALLEL_PRINTER2_IO_MASK) == PARALLEL_PRINTER2_IO_BASE))
		skip_io++;
	else if ((run->io.port & PCI_IO_MASK) == PCI_IO_BASE) {
		/* dump_io_access_begin(run); */

		static unsigned int pci_config_addr;
		switch (run->io.port) {
		case PCI_IO_CONFIG_ADDR:
			if (run->io.direction == KVM_EXIT_IO_OUT) {
				pci_config_addr = *(unsigned int *)((unsigned char *)run + run->io.data_offset);
			}
			break;

		case PCI_IO_CONFIG_DATA:
			if (run->io.direction == KVM_EXIT_IO_IN) {
				if (run->io.size == 2)
					*(unsigned short *)((unsigned char *)run + run->io.data_offset) = 0x8000;
			}
			break;

		default:
			break;
		}

		/* dump_io_access_end(run); */
	} else if (run->io.port == 0x0510 || run->io.port == 0x0511
		 || run->io.port == 0x000d || run->io.port == 0x03e9)
		skip_io++;
	else if ((run->io.port & UPW48_IO_MASK) == UPW48_IO_BASE)
		skip_io++;
	else if ((run->io.port & HDC1_IO_MASK) == HDC1_IO_BASE
		 || (run->io.port & HDC2_IO_MASK) == HDC2_IO_BASE)
		skip_io++;
	/* else { */
	/* 	dump_io_access_begin(run); */
	/* 	fflush(stdout); */
	/* 	assert(0, "undefined io\n"); */
	/* } */

	dump_io_access_end(run);
}
