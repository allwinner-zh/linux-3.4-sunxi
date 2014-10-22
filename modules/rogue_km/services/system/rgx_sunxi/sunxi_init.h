#if !defined(__SUNXI_INIT__)
#define __SUNXI_INIT__
long int GetConfigFreq(IMG_VOID);
IMG_VOID RgxSunxiInit(IMG_VOID);
IMG_VOID RgxResume(IMG_VOID);
IMG_VOID RgxSuspend(IMG_VOID);
PVRSRV_ERROR IonInit(void *pvPrivateData);
IMG_VOID IonDeinit(IMG_VOID);
PVRSRV_ERROR AwPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced);
PVRSRV_ERROR AwPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced);
#endif	/* __SUNXI_INIT__ */
