static void
watchdog_reboot(s32 tick)
{
    *(vu32 *)PM_RSTC = PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
    *(vu32 *)PM_WDOG = PM_PASSWORD | tick;
}

static void
watchdog_cancel_reboot(void)
{
    *(vu32 *)PM_RSTC = PM_PASSWORD | 0;
    *(vu32 *)PM_WDOG = PM_PASSWORD | 0;
}
