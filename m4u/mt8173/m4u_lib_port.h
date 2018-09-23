#ifndef __M4U_LIB_PORT_8173_H__
#define __M4U_LIB_PORT_8173_H__

//====================================
// about portid
//====================================

enum
{
    /*larb0*/
    M4U_PORT_DISP_OVL0           ,
    M4U_PORT_DISP_RDMA0          ,
    M4U_PORT_DISP_WDMA0          ,
    M4U_PORT_DISP_OD_R           ,
    M4U_PORT_DISP_OD_W           ,
    M4U_PORT_MDP_RDMA0           ,
    M4U_PORT_MDP_WDMA            ,
    M4U_PORT_MDP_WROT0           ,

    /*larb1*/
    M4U_PORT_HW_VDEC_MC_EXT      ,
    M4U_PORT_HW_VDEC_PP_EXT      ,
    M4U_PORT_HW_VDEC_UFO_EXT     ,
    M4U_PORT_HW_VDEC_VLD_EXT     ,
    M4U_PORT_HW_VDEC_VLD2_EXT    ,
    M4U_PORT_HW_VDEC_AVC_MV_EXT ,
    M4U_PORT_HW_VDEC_PRED_RD_EXT ,
    M4U_PORT_HW_VDEC_PRED_WR_EXT ,
    M4U_PORT_HW_VDEC_PPWRAP_EXT  ,
    M4U_PORT_HW_VDEC_TILE        ,

    /*larb2*/
    M4U_PORT_IMGO                ,
    M4U_PORT_RRZO                ,
    M4U_PORT_AAO                 ,
    M4U_PORT_LCSO               ,
    M4U_PORT_ESFKO               ,
    M4U_PORT_IMGO_D              ,
    M4U_PORT_LSCI                ,
    M4U_PORT_LSCI_D              ,
    M4U_PORT_BPCI                ,
    M4U_PORT_BPCI_D              ,
    M4U_PORT_UFDI                ,
    M4U_PORT_IMGI                ,
    M4U_PORT_IMG2O               ,
    M4U_PORT_IMG3O               ,
    M4U_PORT_VIPI                ,
    M4U_PORT_VIP2I               ,
    M4U_PORT_VIP3I               ,
    M4U_PORT_LCEI                ,
    M4U_PORT_RB                  ,
    M4U_PORT_RP                  ,
    M4U_PORT_WR                  ,

    /*larb3*/
    M4U_PORT_VENC_RCPU           ,
    M4U_PORT_VENC_REC            ,
    M4U_PORT_VENC_BSDMA          ,
    M4U_PORT_VENC_SV_COMV        ,
    M4U_PORT_VENC_RD_COMV        ,
    M4U_PORT_JPGENC_RDMA         ,
    M4U_PORT_JPGENC_BSDMA        ,
    M4U_PORT_JPGDEC_WDMA         ,
    M4U_PORT_JPGDEC_BSDMA        ,
    M4U_PORT_VENC_CUR_LUMA       ,
    M4U_PORT_VENC_CUR_CHROMA     ,
    M4U_PORT_VENC_REF_LUMA       ,
    M4U_PORT_VENC_REF_CHROMA     ,
    M4U_PORT_VENC_NBM_RDMA       ,
    M4U_PORT_VENC_NBM_WDMA       ,

    /*larb4*/
    M4U_PORT_DISP_OVL1           ,
    M4U_PORT_DISP_RDMA1          ,
    M4U_PORT_DISP_RDMA2          ,
    M4U_PORT_DISP_WDMA1          ,
    M4U_PORT_MDP_RDMA1           ,
    M4U_PORT_MDP_WROT1           ,

    /*larb5*/
    M4U_PORT_VENC_RCPU_SET2       ,
    M4U_PORT_VENC_REC_FRM_SET2    ,
    M4U_PORT_VENC_REF_LUMA_SET2   ,
    M4U_PORT_VENC_REC_CHROMA_SET2 ,
    M4U_PORT_VENC_BSDMA_SET2      ,
    M4U_PORT_VENC_CUR_LUMA_SET2   ,
    M4U_PORT_VENC_CUR_CHROMA_SET2 ,
    M4U_PORT_VENC_RD_COMA_SET2    ,
    M4U_PORT_VENC_SV_COMA_SET2    ,

    /*perisys iommu*/
    M4U_PORT_RESERVE,
    M4U_PORT_SPM,
    M4U_PORT_MD32,
    M4U_PORT_PTP_THERM,
    M4U_PORT_PWM,
    M4U_PORT_MSDC1,
    M4U_PORT_MSDC2,
    M4U_PORT_SPI0,
    M4U_PORT_NFI,
    M4U_PORT_AUDIO,
    M4U_PORT_RESERVED2,
    M4U_PORT_HSIC_XHCI,

    M4U_PORT_HSIC_MAS,
    M4U_PORT_HSIC_DEV,
    M4U_PORT_AP_DMA,
    M4U_PORT_HSIC_DMA,
    M4U_PORT_MSDC0,
    M4U_PORT_MSDC3,
    /*M4U_PORT_DEBUGTOP1,*/

    M4U_PORT_UNKNOWN,
};

#define M4U_PORT_NR M4U_PORT_UNKNOWN

#endif
