#include "ftdi_usb.h"
#include <furi_hal.h>
#include "ftdi.h"

#define TAG "FTDI USB"

#define FTDI_USB_VID (0x0403)
#define FTDI_USB_PID (0x6014)

#define FTDI_USB_EP_IN (0x81)
#define FTDI_USB_EP_OUT (0x02)

#define FTDI_USB_EP_IN_SIZE (64UL)
#define FTDI_USB_EP_OUT_SIZE (64UL)

#define FTDI_USB_RX_MAX_SIZE (FTDI_USB_EP_OUT_SIZE)
#define FTDI_USB_MODEM_STATUS_SIZE (sizeof(uint16_t))
#define FTDI_USB_TX_MAX_SIZE (FTDI_USB_EP_IN_SIZE - FTDI_USB_MODEM_STATUS_SIZE)

typedef struct {
    uint16_t status;
    uint8_t data[FTDI_USB_TX_MAX_SIZE];
} FtdiTxData;

static usbd_respond ftdi_usb_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond
    ftdi_usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static void ftdi_usb_send(usbd_device* dev, uint8_t* buf, uint16_t len);
static int32_t ftdi_usb_receive(usbd_device* dev, uint8_t* buf, uint16_t max_len);

typedef enum {
    EventExit = (1 << 0),
    EventReset = (1 << 1),
    EventRx = (1 << 2),
    EventTx = (1 << 3),
    EventTxComplete = (1 << 4),
    EventResetSio = (1 << 5),
    EventResetPurgeRx = (1 << 6),
    EventResetPurgeTx = (1 << 7),
    EventSetBitmode = (1 << 8),
    EventSetLatencyTimer = (1 << 9),
    EventSetEventChar = (1 << 10),
    EventSetErrorChar = (1 << 11),
    EventSetBaudrate = (1 << 12),
    EventSetData = (1 << 13),
    EventSetFlowCtrl = (1 << 14),

    EventAll = EventExit | EventReset | EventRx | EventTx | EventTxComplete | EventResetSio |
               EventResetPurgeRx | EventResetPurgeTx | EventSetBitmode | EventSetLatencyTimer |
               EventSetEventChar | EventSetErrorChar | EventSetBaudrate | EventSetData |
               EventSetFlowCtrl,
} FtdiEvent;

struct FtdiUsb {
    FuriHalUsbInterface usb;
    FuriHalUsbInterface* usb_prev;

    FuriThread* thread;
    usbd_device* dev;
    Ftdi* ftdi;
    uint8_t data_recvest[8];
    uint16_t data_recvest_len;
};

static int32_t ftdi_thread_worker(void* context) {
    FtdiUsb* ftdi_usb = context;
    usbd_device* dev = ftdi_usb->dev;
    UNUSED(dev);

    uint32_t len_data = 0;
    FtdiTxData tx_data = {0};
    // uint16_t *status = 0;
    // ftdi_get_modem_status(&status);
    uint16_t* status = ftdi_get_modem_status_uint16_t(ftdi_usb->ftdi);

    tx_data.status = status[0];
    ftdi_usb_send(dev, (uint8_t*)&tx_data, FTDI_USB_MODEM_STATUS_SIZE);

    while(true) {
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);

        //furi_hal_gpio_write(&gpio_ext_pa7, 1);

        if(flags & EventRx) { //fast flag
            uint8_t buf[FTDI_USB_RX_MAX_SIZE];
            len_data = ftdi_usb_receive(dev, buf, FTDI_USB_RX_MAX_SIZE);
            // if(len_data > 0) {
            //     for(uint32_t i = 0; i < len_data; i++) {
            //         FURI_LOG_RAW_I("%c", (char)buf[i]);
            //     }
            //     FURI_LOG_RAW_I("\r\n");
            // }
            if(len_data > 0) {
                ftdi_set_rx_buf(ftdi_usb->ftdi, buf, len_data);
                //ftdi_loopback(ftdi_usb->ftdi);
                ftdi_start_uart_tx(ftdi_usb->ftdi);
            }
            flags &= ~EventRx; // clear flag
        }

        if(flags) {
            if(flags & EventResetSio) {
                furi_log_puts("EventResetSio\r\n");
                ftdi_reset_sio(ftdi_usb->ftdi);
                ftdi_usb_send(dev, (uint8_t*)&tx_data, FTDI_USB_MODEM_STATUS_SIZE);
            }
            if(flags & EventTxComplete) {
                flags |= EventTx;
            }
            if(flags & EventTx) {
                tx_data.status = status[0];
                len_data = ftdi_available_tx_buf(ftdi_usb->ftdi);

                if(len_data > 0) {
                    if(len_data > FTDI_USB_TX_MAX_SIZE) {
                        len_data = FTDI_USB_TX_MAX_SIZE;
                    }
                    len_data = ftdi_get_tx_buf(ftdi_usb->ftdi, tx_data.data, len_data);
                    ftdi_usb_send(dev, (uint8_t*)&tx_data, len_data + FTDI_USB_MODEM_STATUS_SIZE);
                } else {
                    ftdi_usb_send(dev, (uint8_t*)&tx_data, FTDI_USB_MODEM_STATUS_SIZE);
                }
            }

            // if(flags & EventResetPurgeRx) {
            //     ftdi_reset_purge_rx(ftdi_usb->ftdi);
            // }
            // if(flags & EventResetPurgeTx) {
            //     ftdi_reset_purge_tx(ftdi_usb->ftdi);
            // }

            if(flags & EventExit) {
                FURI_LOG_I(TAG, "exit");
                break;
            }
        }

        // furi_hal_gpio_write(&gpio_ext_pa7, 0);
    }

    return 0;
}

// needed in ftdi_usb_deinit, ftdi_usb_suspend, usb_rxtx_ep_callback, ftdi_usb_control,
// where if_ctx isn't passed
static FtdiUsb* ftdi_cur = NULL;

static void ftdi_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    FtdiUsb* ftdi_usb = ctx;
    ftdi_cur = ftdi_usb;
    ftdi_usb->dev = dev;

    usbd_reg_config(dev, ftdi_usb_ep_config);
    usbd_reg_control(dev, ftdi_usb_control);
    usbd_connect(dev, true);

    ftdi_usb->thread = furi_thread_alloc();
    furi_thread_set_name(ftdi_usb->thread, "FtdiUsb");
    furi_thread_set_stack_size(ftdi_usb->thread, 1024);
    furi_thread_set_context(ftdi_usb->thread, ctx);
    furi_thread_set_callback(ftdi_usb->thread, ftdi_thread_worker);

    ftdi_usb->ftdi = ftdi_alloc();

    // furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    // furi_hal_gpio_init(&gpio_ext_pa6, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);

    furi_thread_start(ftdi_usb->thread);
}

static void ftdi_usb_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    FtdiUsb* ftdi_usb = ftdi_cur;
    if(!ftdi_usb || ftdi_usb->dev != dev) {
        return;
    }
    ftdi_cur = NULL;

    furi_assert(ftdi_usb->thread);
    furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventExit);
    furi_thread_join(ftdi_usb->thread);
    furi_thread_free(ftdi_usb->thread);
    ftdi_usb->thread = NULL;

    ftdi_free(ftdi_usb->ftdi);

    //furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    //furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    free(ftdi_usb->usb.str_prod_descr);
    ftdi_usb->usb.str_prod_descr = NULL;
    free(ftdi_usb->usb.str_serial_descr);
    ftdi_usb->usb.str_serial_descr = NULL;
    free(ftdi_usb);
}

static void ftdi_usb_send(usbd_device* dev, uint8_t* buf, uint16_t len) {
    usbd_ep_write(dev, FTDI_USB_EP_IN, buf, len);
}

static int32_t ftdi_usb_receive(usbd_device* dev, uint8_t* buf, uint16_t max_len) {
    int32_t len = usbd_ep_read(dev, FTDI_USB_EP_OUT, buf, max_len);
    return ((len < 0) ? 0 : len);
}

static void ftdi_usb_wakeup(usbd_device* dev) {
    UNUSED(dev);
}

static void ftdi_usb_suspend(usbd_device* dev) {
    FtdiUsb* ftdi_usb = ftdi_cur;
    if(!ftdi_usb || ftdi_usb->dev != dev) return;
    // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventReset);
}

// static void ftdi_usb_rxtx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
//     UNUSED(ep);
//     UNUSED(event);
//     FtdiUsb* ftdi_usb = ftdi_cur;
//     if(!ftdi_usb || ftdi_usb->dev != dev) return;
//     // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventRxTx);
// }

static void ftdi_usb_rx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    FtdiUsb* ftdi_usb = ftdi_cur;
    //furi_hal_gpio_write(&gpio_ext_pa6, 1);
    furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventRx);

    // uint8_t buf[FTDI_USB_RX_MAX_SIZE];
    // uint32_t len_data = ftdi_usb_receive(dev, buf, FTDI_USB_RX_MAX_SIZE);
    // if(len_data > 0) {
    //     ftdi_set_rx_buf(ftdi_usb->ftdi, buf, len_data);
    //     // ftdi_loopback(ftdi_usb->ftdi);
    // }
    //furi_hal_gpio_write(&gpio_ext_pa6, 0);
}

static void ftdi_usb_tx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    FtdiUsb* ftdi_usb = ftdi_cur;
    furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventTxComplete);
}

static usbd_respond ftdi_usb_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0: // deconfig
        usbd_ep_deconfig(dev, FTDI_USB_EP_OUT);
        usbd_ep_deconfig(dev, FTDI_USB_EP_IN);
        usbd_reg_endpoint(dev, FTDI_USB_EP_OUT, NULL);
        usbd_reg_endpoint(dev, FTDI_USB_EP_IN, NULL);
        return usbd_ack;
    case 1: // config
        usbd_ep_config(
            dev, FTDI_USB_EP_IN, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF*/, FTDI_USB_EP_IN_SIZE);
        usbd_ep_config(
            dev, FTDI_USB_EP_OUT, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF*/, FTDI_USB_EP_OUT_SIZE);
        usbd_reg_endpoint(dev, FTDI_USB_EP_IN, ftdi_usb_tx_ep_callback);
        usbd_reg_endpoint(dev, FTDI_USB_EP_OUT, ftdi_usb_rx_ep_callback);
        return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond
    ftdi_usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    UNUSED(dev);

    if(!(req->bmRequestType & (FtdiControlTypeVendor | FtdiControlRecipientDevice))) {
        return usbd_fail;
    }

    FtdiUsb* ftdi_usb = ftdi_cur;
    if(!ftdi_usb || ftdi_usb->dev != dev) {
        return usbd_fail;
    }

    furi_log_puts("-----------\r\n");

    char tmp_str[] = "0xFFFFFFFF";
    itoa(req->bmRequestType, tmp_str, 16);
    furi_log_puts(tmp_str);
    furi_log_puts(" ");

    itoa(req->bRequest, tmp_str, 16);
    furi_log_puts(tmp_str);
    furi_log_puts(" ");

    itoa(req->wValue, tmp_str, 16);
    furi_log_puts(tmp_str);
    furi_log_puts(" ");

    itoa(req->wIndex, tmp_str, 16);
    furi_log_puts(tmp_str);
    furi_log_puts(" ");

    itoa(req->wLength, tmp_str, 16);
    furi_log_puts(tmp_str);
    furi_log_puts(" \r\n");

    // if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) !=
    //    (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
    //     return usbd_fail;
    // }

    switch(req->bmRequestType) {
    case FtdiControlRequestsOut:

        switch(req->bRequest) {
        case FtdiRequestsSiOReqReset:
            furi_log_puts("ftdi_usb_control OUT\r\n");
            if(req->wValue == FtdiResetSio) {
                furi_log_puts("FtdiResetSio\r\n");
                //ftdi_reset_sio(ftdi_usb->ftdi);
                furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventResetSio);
            }
            if(req->wValue == FtdiResetPurgeRx) {
                furi_log_puts("FtdiResetPurgeRx\r\n");
                ftdi_reset_purge_rx(ftdi_usb->ftdi);

                //furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventResetPurgeRx);
            }
            if(req->wValue == FtdiResetPurgeTx) {
                furi_log_puts("FtdiResetPurgeTx\r\n");
                ftdi_reset_purge_tx(ftdi_usb->ftdi);
                //furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventResetPurgeTx);
            }
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetBitmode:
            furi_log_puts("FtdiRequestsSiOReqSetBitmode\r\n");
            ftdi_set_bitmode(ftdi_usb->ftdi, req->wValue, req->wIndex);
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetBitmode);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetLatencyTimer:
            furi_log_puts("FtdiRequestsSiOReqSetLatencyTimer\r\n");
            ftdi_set_latency_timer(ftdi_usb->ftdi, req->wValue, req->wIndex);
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetLatencyTimer);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetEventChar:
            furi_log_puts("FtdiRequestsSiOReqSetEventChar\r\n");
            //value?????? bool enable:  value |= 1 << 8
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetEventChar);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetErrorChar:
            furi_log_puts("FtdiRequestsSiOReqSetErrorChar\r\n");
            //value?????? bool enable:  value |= 1 << 8
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetErrorChar);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetBaudrate:
            furi_log_puts("FtdiRequestsSiOReqSetBaudrate\r\n");
            ftdi_set_baudrate(ftdi_usb->ftdi, req->wValue, req->wIndex);
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetBaudrate);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetData:
            furi_log_puts("FtdiRequestsSiOReqSetData\r\n");
            ftdi_set_data_config(ftdi_usb->ftdi, req->wValue, req->wIndex);
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetData);
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqSetFlowCtrl:
            furi_log_puts("FtdiRequestsSiOReqSetFlowCtrl\r\n");
            ftdi_set_flow_ctrl(ftdi_usb->ftdi, req->wIndex);
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetFlowCtrl);
            return usbd_ack;
            break;
        default:
            break;
        }

        break;
    case FtdiControlRequestsIn:
        furi_log_puts("ftdi_usb_control IN\r\n");
        switch(req->bRequest) {
        case FtdiRequestsSiOReqGetLatencyTimer:
            furi_log_puts("FtdiRequestsSiOReqGetLatencyTimer\r\n");
            ftdi_usb->data_recvest[0] = ftdi_get_latency_timer(ftdi_usb->ftdi);
            ftdi_usb->data_recvest_len = 1;
            ftdi_usb->dev->status.data_ptr = ftdi_usb->data_recvest;
            ftdi_usb->dev->status.data_count = ftdi_usb->data_recvest_len;
            // furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventSetLatencyTimer);
            return usbd_ack;
            break;

        case FtdiRequestsSiOReqPollModemStatus:
            furi_log_puts("FtdiRequestsSiOReqPollModemStatus\r\n");
            uint16_t* status = ftdi_get_modem_status_uint16_t(ftdi_usb->ftdi);
            memcpy(ftdi_usb->data_recvest, status, sizeof(uint16_t));
            ftdi_usb->data_recvest_len = req->wLength;
            ftdi_usb->dev->status.data_ptr = ftdi_usb->data_recvest;
            ftdi_usb->dev->status.data_count = ftdi_usb->data_recvest_len;
            return usbd_ack;
            break;
        case FtdiRequestsSiOReqReadPins:
            furi_log_puts("FtdiRequestsSiOReqReadPins\r\n");
            ftdi_usb->data_recvest[0] = ftdi_get_bitbang_gpio(ftdi_usb->ftdi);
            ftdi_usb->data_recvest_len = 1;
            ftdi_usb->dev->status.data_ptr = ftdi_usb->data_recvest;
            ftdi_usb->dev->status.data_count = ftdi_usb->data_recvest_len;
            return usbd_ack;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    // switch(req->bRequest) {
    // case USB_MSC_BOT_GET_MAX_LUN: {
    //     static uint8_t max_lun = 0;
    //     dev->status.data_ptr = &max_lun;
    //     dev->status.data_count = 1;
    //     return usbd_ack;
    // }; break;
    // case USB_MSC_BOT_RESET: {
    //     FtdiUsb* ftdi_usb = ftdi_cur;
    //     if(!ftdi_usb || ftdi_usb->dev != dev) return usbd_fail;
    //     furi_thread_flags_set(furi_thread_get_id(ftdi_usb->thread), EventReset);
    //     return usbd_ack;
    // }; break;
    // }
    return usbd_fail;
}

static const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC("FTDI");
static const struct usb_string_descriptor dev_product_desc = USB_STRING_DESC("FlipTDI");

struct FtdiUsbDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
} __attribute__((packed));

static const struct usb_device_descriptor usb_ftdi_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8, // USB_EP0_SIZE
    .idVendor = FTDI_USB_VID,
    .idProduct = FTDI_USB_PID,
    .bcdDevice = VERSION_BCD(9, 0, 0),
    .iManufacturer = 1, // UsbDevManuf
    .iProduct = 2, // UsbDevProduct
    .iSerialNumber = NO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

static const struct FtdiUsbDescriptor usb_ftdi_cfg_descr = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct FtdiUsbDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_VENDOR,
            .bInterfaceSubClass = USB_SUBCLASS_VENDOR,
            .bInterfaceProtocol = USB_PROTO_VENDOR,
            .iInterface = 2,
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = FTDI_USB_EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = FTDI_USB_EP_IN_SIZE,
            .bInterval = 0,
        },
    .ep_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = FTDI_USB_EP_OUT,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = FTDI_USB_EP_OUT_SIZE,
            .bInterval = 0,
        },
};

FtdiUsb* ftdi_usb_start(void) {
    FtdiUsb* ftdi_usb = malloc(sizeof(FtdiUsb));

    ftdi_usb->usb_prev = furi_hal_usb_get_config();
    ftdi_usb->usb.init = ftdi_usb_init;
    ftdi_usb->usb.deinit = ftdi_usb_deinit;
    ftdi_usb->usb.wakeup = ftdi_usb_wakeup;
    ftdi_usb->usb.suspend = ftdi_usb_suspend;
    ftdi_usb->usb.dev_descr = (struct usb_device_descriptor*)&usb_ftdi_dev_descr;
    ftdi_usb->usb.str_manuf_descr = (void*)&dev_manuf_desc;
    ftdi_usb->usb.str_prod_descr = (void*)&dev_product_desc;
    ftdi_usb->usb.str_serial_descr = NULL;
    ftdi_usb->usb.cfg_descr = (void*)&usb_ftdi_cfg_descr;

    if(!furi_hal_usb_set_config(&ftdi_usb->usb, ftdi_usb)) {
        FURI_LOG_E(TAG, "USB locked, cannot start Mass Storage");
        free(ftdi_usb->usb.str_prod_descr);
        free(ftdi_usb->usb.str_serial_descr);
        free(ftdi_usb);
        return NULL;
    }
    return ftdi_usb;
}

void ftdi_usb_stop(FtdiUsb* ftdi_usb) {
    furi_hal_usb_set_config(ftdi_usb->usb_prev, NULL);
}