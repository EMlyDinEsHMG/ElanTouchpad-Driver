/*
 * Elantech Touchpad driver (v1) for Mac OSX
 *
 * Copyright (C) 2012 EMlyDinEsHMG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Trademarks are the property of their respective owners.
 */
#ifndef _APPLEPS2ELANTOUCHPAD_H
#define _APPLEPS2ELANTOUCHPAD_H


/*
 * Command values for Synaptics style queries
 */
#define ETP_FW_ID_QUERY			0x00
#define ETP_FW_VERSION_QUERY		0x01
#define ETP_CAPABILITIES_QUERY		0x02
#define ETP_SAMPLE_QUERY		0x03
#define ETP_RESOLUTION_QUERY		0x04

/*
 * Command values for register reading or writing
 */
#define ETP_REGISTER_READ		0x10
#define ETP_REGISTER_WRITE		0x11
#define ETP_REGISTER_READWRITE		0x00

/*
 * Hardware version 2 custom PS/2 command value
 */
#define ETP_PS2_CUSTOM_COMMAND		0xf8

/*
 * Times to retry a ps2_command and millisecond delay between tries
 */
#define ETP_PS2_COMMAND_TRIES		3
#define ETP_PS2_COMMAND_DELAY		500

/*
 * Times to try to read back a register and millisecond delay between tries
 */
#define ETP_READ_BACK_TRIES		5
#define ETP_READ_BACK_DELAY		2000

/*
 * Register bitmasks for hardware version 1
 */
#define ETP_R10_ABSOLUTE_MODE		0x04
#define ETP_R11_4_BYTE_MODE		0x02

/*
 * Capability bitmasks
 */
#define ETP_CAP_HAS_ROCKER		0x04

/*
 * One hard to find application note states that X axis range is 0 to 576
 * and Y axis range is 0 to 384 for harware version 1.
 * Edge fuzz might be necessary because of bezel around the touchpad
 */
#define ETP_EDGE_FUZZ_V1		32

#define ETP_XMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_XMAX_V1			(576 - ETP_EDGE_FUZZ_V1)
#define ETP_YMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_YMAX_V1			(384 - ETP_EDGE_FUZZ_V1)

/*
 * The resolution for older v2 hardware doubled.
 * (newer v2's firmware provides command so we can query)
 */
#define ETP_XMIN_V2			0
#define ETP_XMAX_V2			1152
#define ETP_YMIN_V2			0
#define ETP_YMAX_V2			768

#define ETP_PMIN_V2			0
#define ETP_PMAX_V2			255
#define ETP_WMIN_V2			0
#define ETP_WMAX_V2			15

/*
 * v3 hardware has 2 kinds of packet types,
 * v4 hardware has 3.
 */
#define PACKET_UNKNOWN			0x01
#define PACKET_DEBOUNCE			0x02
#define PACKET_V3_HEAD			0x03
#define PACKET_V3_TAIL			0x04
#define PACKET_V4_HEAD			0x05
#define PACKET_V4_MOTION		0x06
#define PACKET_V4_STATUS		0x07

//Custom
#define PACKET_V3_THREE_FINGER_TAP		0xAA
/*
 * track up to 5 fingers for v4 hardware
 */
#define ETP_MAX_FINGERS			5

/*
 * weight value for v4 hardware
 */
#define ETP_WEIGHT_VALUE		5

/*** Absolute values from Linux*/
#define ABS_X                   0x00
#define ABS_Y                   0x01

enum {
    //
    //
    kTapEnabled  = 0x01
};

typedef struct ELANStatus
{
    UInt8 Byte1;
    UInt8 Byte2;
    UInt8 Byte3;
} ELANStatus_type;


int scrollFactor = 1; //TwoFingerScroll Factor


#include "ApplePS2MouseDevice.h"
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ApplePS2ElanTouchPad Class Declaration
//

class ApplePS2ElanTouchPad : public IOHIPointing
{
	OSDeclareDefaultStructors( ApplePS2ElanTouchPad );
    
private:
    ApplePS2MouseDevice * _device;
    
    UInt32                _interruptHandlerInstalled:1;
    UInt32                _powerControlHandlerInstalled:1;
    UInt32                _keyboardNotificationHandlerInstalled:1;
    
    UInt8                 _packetBuffer[50];
    UInt32                _packetByteCount;
    IOFixed               _resolution;
    UInt16                _touchPadVersion;
    UInt8                 _touchPadModeByte;
    
    bool                  _disableTouch, _enableTypingMode, _accidentalInput, _mouseBtnsEnableTouch;
    bool                  _enableEdgeCirular, _enablePinchZoom, _enableRotate,_enableSwipeLR, _enableSwipeUpDwn;
    bool                  _enableCornerTaps, _singleDoubleTapDrag, fasterMode;
    
    bool				  _dragging, _mouseDPI;
	bool				  _edgehscroll;
	bool				  _edgevscroll;
    UInt32                _edgeaccell;
    double                _edgeaccellvalue;
	bool				  _draglock;
    bool                  _clicking, _buttonSwap;
    uint64_t _maxclicktiming;
    bool    _StartTracking;
    
    unsigned int                _ymax;
    unsigned int                _xmax;
    unsigned int                _ymin;
    unsigned int                _xmin;
    int _xrest;
    int _yrest;
    int _lastX;
    int _lastY;
    int _initX;
    int _initY;
    int _lastX1, _lastY1, _lastX2, _lastY2, _startX1, _startY1, _startX2, _startY2;
    UInt32 buttons;
    
    uint64_t maxtaptime, maxclicktime;
	uint64_t maxdragtime;
    uint64_t curTouchtime, singleTapTouchTime;
    uint64_t lastTouchtime, lastKeyPressTime, accInputTimeOut;

    bool clicking, dragging, draglock, dragReleased, dragStarted,tempDisableCornerTaps, dragged;
    bool _jitter, _corners,_threeFingTap, _twoFingDTap;
	bool hscroll, scroll, TwoFingerScroll, fingersPressed;
    bool rbtn, lbtnDrag,buttonTriggered, midBtn, trackpadStarted, circularStarted, dragPressure;;
    bool swipeDownDone, swipeUpDone, swipeLeftDone, swipeRightDone, zoomDone, zoomOut, zoomIn;
    bool cornerTapped, corner, cornerTopLeft, cornerBottomLeft, cornerTopRight, cornerBottomRight;
    bool  dirLeft, dirRight, rotateLeft, rotateRight, rotateCirLeft, rotateCirRight, rotateDone, rotateMode;
    
	enum {MODE_NO_MOVE, MODE_MOVE, MODE_VSCROLL, MODE_HSCROLL, MODE_CSCROLL, MODE_3_FING_TAP, MODE_MUL_TOUCH, MODE_HOLD_DRAG, MODE_DRAG, MODE_TAP, MODE_2_FING_TAP, MODE_EDGE_VSCROLL, MODE_EDGE_HSCROLL, MODE_CIR_HSCROLL, MODE_CIR_VSCROLL, MODE_BUTTONS, MODE_CORNER_TAP, MODE_THREE_FING_PRESS, MODE_TWO_FINGERS_PRESS, MODE_ROTATE, MODE_ZOOM} touchmode;
    
    int  cameFrom, taps, clicks, tapDragDelayCount, holdDragDelayCount,ScrollDelayCount, slowScrollDelay;
    int _xdiff, _ydiff, _xdiff_2, _ydiff_2, _xdiff_1, _ydiff_1, s_xdiff, s_ydiff;
    int hgsscrollCounter, hlsscrollCounter, vgsscrollCounter, vlsscrollCounter, vScroll, hScroll;
    int  zoomXDiff, zoomYDiff, rotateXStart, rotateYStart, rotateXCounter, rotateYCounter, partialRotateR, partialRotateL;
    int  centerx, centery, divisor, accidentalInputKeys;
    UInt64 swipeUpAction, swipeDownAction ;
   
    /*
     * The base position for one finger, v4 hardware
     */
    unsigned int finger_pos_x;
    unsigned int finger_pos_y;
	/*
     * Data
     */
    unsigned char reg_07;
	unsigned char reg_10;
	unsigned char reg_11;
	unsigned char reg_20;
	unsigned char reg_21;
	unsigned char reg_22;
	unsigned char reg_23;
	unsigned char reg_24;
	unsigned char reg_25;
	unsigned char reg_26;
	unsigned char debug;
	unsigned char capabilities[3];
	bool paritycheck;
	bool jumpy_cursor;
	bool reports_pressure;
	unsigned char hw_version;
	unsigned int fw_version;
	unsigned int single_finger_reports;
	unsigned int width, fingers;
    unsigned int lastCordinatesX;
    unsigned int lastCordinatesY;
    int pktsize, track, lastFingersNum, pressure, lastPressure;
	unsigned char parity[256];
    
    int (*send_cmd)(ApplePS2MouseDevice *device, unsigned char c, unsigned char *param);

	virtual void   free();
	virtual void   interruptOccurred( UInt8 data );
    virtual void   setDevicePowerState(UInt32 whatToDo);
    //Receiving Notifications
    virtual void   receiveKeyboardNotifications(UInt32 data);
    
    virtual int Elantech_Slice_command(ApplePS2MouseDevice *device, unsigned char c);
    static int Synaptics_send_cmd(ApplePS2MouseDevice *device, unsigned char c, unsigned char *param);
    static int Elantech_send_cmd(ApplePS2MouseDevice *device, unsigned char c, unsigned char *param);
    virtual int Elantech_ps2_cmd(ApplePS2MouseDevice *device, unsigned char *param, unsigned char c);
    virtual int Generic_ps2_cmd(ApplePS2MouseDevice *device, unsigned char *param,unsigned char c);
    
    virtual int Elantech_read_reg(ApplePS2MouseDevice *device, unsigned char reg, unsigned char *val);
    virtual int Elantech_write_reg(ApplePS2MouseDevice *device, unsigned char reg, unsigned char *val);
        
    //virtual void Elantech_report_absolute_v1(ApplePS2MouseDevice *device);
    //virtual void Elantech_report_absolute_v2(ApplePS2MouseDevice *device);
    virtual void Elantech_report_absolute_v3(int packet_type , unsigned char *packets);
    //virtual void Elantech_report_absolute_v4(ApplePS2MouseDevice *device, int packet_type);
    
    /*virtual void Elantech_input_sync_v4(ApplePS2MouseDevice *device);
     virtual void Process_packet_status_v4(ApplePS2MouseDevice *device);
     virtual void Process_packet_head_v4(ApplePS2MouseDevice *device);
     virtual void Process_packet_motion_v4(ApplePS2MouseDevice *device);
     
     virtual int Elantech_packet_check_v1(ApplePS2MouseDevice *device);
     virtual int Elantech_debounce_check_v2(ApplePS2MouseDevice *device);
     virtual int Elantech_packet_check_v2(ApplePS2MouseDevice *device);*/
    //virtual int Elantech_packet_check_v3(UInt8 packets);
    //virtual int Elantech_packet_check_v4(ApplePS2MouseDevice *device);
    
    virtual void   setCommandByte( UInt8 setBits, UInt8 clearBits );
    virtual bool   setTouchPadModeByte( UInt8 modeByteValue,
                                       bool  enableStreamMode = false );
    virtual void Process_twofingers_touch(int x1, int x2, int y1, int y2);
    virtual void Process_singlefinger_touch(int x, int y);
    virtual void Process_Threefingers_touch(int midfinger_x, int midfinger_y);
    virtual void Process_End_functions(int packet_type, unsigned char *packets);
    
protected:
	
    virtual int Elantech_set_absolute_mode(ApplePS2MouseDevice *device);
    virtual int Elantech_set_range(ApplePS2MouseDevice *device,
                                   unsigned int *x_min, unsigned int *y_min,
                                   unsigned int *x_max, unsigned int *y_max,
                                   unsigned int *width);
    virtual unsigned int Elantech_convert_res(unsigned int val);
    virtual int Elantech_get_resolution_v4(ApplePS2MouseDevice *device,
                                           unsigned int *x_res,
                                           unsigned int *y_res);
    virtual void Elantech_Touchpad_enable( bool enable);
    
public:
    virtual bool init( OSDictionary * properties );
    virtual ApplePS2ElanTouchPad * probe( IOService * provider,
                                         SInt32 *    score );
    
    virtual int Elantech_detect(ApplePS2MouseDevice *device);
    virtual int Elantech_set_properties(ApplePS2MouseDevice *device);
    virtual bool Elantech_is_signatures_valid(const unsigned char *param);
    
    virtual bool start( IOService * provider );
    virtual void stop( IOService * provider );
    
    virtual IOReturn setParamProperties( OSDictionary * dict );
	virtual IOReturn setProperties (OSObject *props);
    virtual bool updateProperties(void);
    
    virtual void getStatus(ELANStatus_type *status);
    virtual void getMouseInformation();
    virtual void setSampleRateAndResolution( void );
    virtual void setTapEnable( bool enable );
    virtual int resolution();
    
};
#endif /* _APPLEPS2ELANTOUCHPAD_H */