#ifndef _NDIS_H_

#define _NDIS_H_

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>

namespace s2e {
namespace windows {

#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_FAILURE 0xC0000001L
#define NDIS_STATUS_RESOURCES 0xc000009a
#define NDIS_STATUS_RESOURCE_CONFLICT 0xc001001E

#define NDIS_STATUS_MEDIA_CONNECT               (0x4001000BL)
#define NDIS_STATUS_MEDIA_DISCONNECT            (0x4001000CL)


#define OID_GEN_MEDIA_CONNECT_STATUS 0x00010114

#define NDIS_ERROR_CODE unsigned long
typedef uint32_t NDIS_HANDLE, *PNDIS_HANDLE;

typedef int NDIS_STATUS, *PNDIS_STATUS; // note default size

typedef struct _NDIS_MINIPORT_CHARACTERISTICS32 {
    uint8_t MajorNdisVersion;
    uint8_t MinorNdisVersion;
    uint32_t Reserved;
    uint32_t CheckForHangHandler;
    uint32_t DisableInterruptHandler;
    uint32_t EnableInterruptHandler;
    uint32_t HaltHandler;
    uint32_t HandleInterruptHandler;
    uint32_t InitializeHandler;
    uint32_t ISRHandler;
    uint32_t QueryInformationHandler;
    uint32_t ReconfigureHandler;
    uint32_t ResetHandler;
    uint32_t SendHandler; 
    uint32_t SetInformationHandler;
    uint32_t TransferDataHandler;
//
// Version used is V4.0 or V5.0
// with following members
//
    uint32_t ReturnPacketHandler;
    uint32_t SendPacketsHandler;
    uint32_t AllocateCompleteHandler;
//
// Version used is V5.0 with the following members
//
    uint32_t CoCreateVcHandler;
    uint32_t CoDeleteVcHandler;
    uint32_t CoActivateVcHandler;
    uint32_t CoDeactivateVcHandler;
    uint32_t CoSendPacketsHandler;
    uint32_t CoRequestHandler;
//
// Version used is V5.1 with the following members
//
    uint32_t CancelSendPacketsHandler;
    uint32_t PnPEventNotifyHandler;
    uint32_t AdapterShutdownHandler;
} NDIS_MINIPORT_CHARACTERISTICS32, *PNDIS_MINIPORT_CHARACTERISTICS32;

typedef s2e::windows::UNICODE_STRING32 NDIS_STRING, *PNDIS_STRING;

typedef enum _NDIS_PARAMETER_TYPE {
  NdisParameterInteger=0,
  NdisParameterHexInteger,
  NdisParameterString,
  NdisParameterMultiString,
  NdisParameterBinary,
} NDIS_PARAMETER_TYPE, *PNDIS_PARAMETER_TYPE;


typedef struct _NDIS_CONFIGURATION_PARAMETER {
    NDIS_PARAMETER_TYPE ParameterType;
    union {
        uint32_t IntegerData;
        NDIS_STRING StringData;
    } ParameterData;
} NDIS_CONFIGURATION_PARAMETER, *PNDIS_CONFIGURATION_PARAMETER;




typedef enum _NDIS_INTERFACE_TYPE
{
    NdisInterfaceInternal = Internal,
    NdisInterfaceIsa = Isa,
    NdisInterfaceEisa = Eisa,
    NdisInterfaceMca = MicroChannel,
    NdisInterfaceTurboChannel = TurboChannel,
    NdisInterfacePci = PCIBus,
    NdisInterfacePcMcia = PCMCIABus,
    NdisInterfaceCBus = CBus,
    NdisInterfaceMPIBus = MPIBus,
    NdisInterfaceMPSABus = MPSABus,
    NdisInterfaceProcessorInternal = ProcessorInternal,
    NdisInterfaceInternalPowerBus = InternalPowerBus,
    NdisInterfacePNPISABus = PNPISABus,
    NdisInterfacePNPBus = PNPBus,
    NdisMaximumInterfaceType
} NDIS_INTERFACE_TYPE, *PNDIS_INTERFACE_TYPE;


typedef enum _NDIS_MEDIUM {
  NdisMedium802_3,
  NdisMedium802_5,
  NdisMediumWan,
  NdisMediumDix,
  NdisMediumWirelessWan,
  NdisMediumIrda,
  NdisMediumBpc,
  NdisMediumCoWan,
  NdisMedium1394,
  NdisMediumMax,
} NDIS_MEDIUM, *PNDIS_MEDIUM;

typedef
NDIS_STATUS
(*W_INITIALIZE_HANDLER)(
    PNDIS_STATUS            OpenErrorStatus,  //OUT
    /*PUINT*/ uint32_t                   SelectedMediumIndex, //OUT
    PNDIS_MEDIUM            MediumArray,  
    uint32_t                    MediumArraySize,
    NDIS_HANDLE             MiniportAdapterContext,
    NDIS_HANDLE             WrapperConfigurationContext
    );




}
}


#endif
