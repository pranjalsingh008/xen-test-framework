/**
 * @file tests/kfx/main.c
 * @ref test-kfx
 *
 * @page test-kfx kfx
 *
 * @todo Docs for test-kfx
 *
 * @see tests/kfx/main.c
 */
#include <xtf.h>

const char test_title[] = "Test kfx";

static struct xen_sysctl cm;

static inline void harness(unsigned int magic, void *a, size_t s)
{
    asm volatile ("cpuid"
                  : "=a" (magic), "=c" (magic), "=S" (magic)
                  : "a" (magic), "c" (s), "S" (a)
                  : "bx", "dx");
}

void test_main(void)
{
    int interface_version_sysctl = xtf_probe_sysctl_interface_version(); 
    
    if ( interface_version_sysctl < 0 )
        return xtf_error("Failed to get sysctl version\n");
    
    printk("Sysctl version: %#x. Struct @ %p size %lu\n", interface_version_sysctl, &cm, sizeof(cm));

    harness(0x13371337, &cm, sizeof(cm));

    cm.cmd = XEN_SYSCTL_scheduler_op,
    cm.interface_version = interface_version_sysctl;
    hypercall_sysctl(&cm);

    harness(0x13371337, NULL, 0);

    xtf_success("Fuzzing done\n");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
