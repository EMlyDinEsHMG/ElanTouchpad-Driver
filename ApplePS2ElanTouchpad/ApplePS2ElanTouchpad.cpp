/*
 * Elantech Touchpad driver (v1) for Mac OSX
 *
 * Copyright (C) 2012 by EMlyDinEsHMG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Trademarks are the property of their respective owners.
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include "ApplePS2ElanTouchpad.h"

//#include <CoreFoundation/CoreFoundation.h>


// =============================================================================
// ApplePS2ElanTouchPad Class Implementation
//
#define DEBUG_START 0

#if DEBUG_START
#define DEBUG_LOG(fmt, args...) IOLog(fmt, ## args)
#else
#define DEBUG_LOG(fmt, args...)
#endif

#define super IOHIPointing
OSDefineMetaClassAndStructors(ApplePS2ElanTouchPad, IOHIPointing);

IOFixed     ApplePS2ElanTouchPad::resolution()  { return _resolution; };


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


bool ApplePS2ElanTouchPad::init( OSDictionary * properties )
{
    //
    // Initialize this object's minimal state. This is invoked right after this
    // object is instantiated.
    //
    //    OSObject *tmp;
	
    if (!super::init(properties))  return false;
    
    _device                    = 0;
    _interruptHandlerInstalled = false;
    _packetByteCount           = 0;
    _resolution                = (1200) << 16; // 1200 dpi default was (100 dpi, 4 counts/mm)
    _mouseDPI                  = 1200;
    _touchPadModeByte          = 0x80; //default: absolute, low-rate, no w-mode
    divisor=1; // Standard was 23, changed for high res fix
    _StartTracking = true;
    fingers = 0;
    _xmin   = 0;
    _ymax   = 0;
    _xmax   = 0;
    _ymax   = 0;
    _xrest  = 0;
    _yrest  = 0;
    _lastX  = 0;
    _lastY  = 0;
    _ydiff = 0;
    _xdiff = 0;
    s_xdiff = 0;
    s_ydiff = 0;
    rotateXStart = rotateYStart = 0;
    rotateXCounter = rotateYCounter = 0;
    partialRotateR = partialRotateL = 0;
    _startX1 = _startX2 = _startY1 = _startY2 = 0;
    centerx = 1250;//V3 Hardware of Mine
    centery = 650;//V3 Hardware of Mine
	maxtaptime=150000000;
    maxclicktime = 100000000;
	maxdragtime=300000000;
    accInputTimeOut = 0;
    
    clicking=false;//For Tapping Two and Three Fingers
	dragging=false;
	draglock=false;
	hscroll=false;
	scroll=false;
    
    taps = 0;
    clicks = 0;
    track = 0;
    curTouchtime = 0;
    lastTouchtime = 0;
    buttons = 0;
    ScrollDelayCount = 0;
    slowScrollDelay = 0;
    hgsscrollCounter = 0;
    vgsscrollCounter = 0;
    hlsscrollCounter = 0;
    vlsscrollCounter = 0;
    hscroll = 0;
    vScroll = 0;
    cameFrom = 0;
    tapDragDelayCount = 0;
    holdDragDelayCount = 0;
    lastPressure = 0;
    lastFingersNum = 0;
    accidentalInputKeys = 0;
    pressure = -1;
    
    buttonTriggered = false;
    lbtnDrag = false;
    rbtn = false;
    midBtn = false;
    threeFingerMode = false;
    TwoFingerScroll = false;
    circularStarted  = false;
    tempDisableCornerTaps = false;
    cornerTapped = false;
    dragReleased = true;
    dragStarted  = false;
    dragged = false;
    fingersPressed = false;
    
    fasterMode = true;
    _enableEdgeCirular = false;
    _enableTypingMode = false;
    _enableCornerTaps= false;
    _buttonSwap = false;
    _accidentalInput = false;

    cornerTopLeft = cornerBottomLeft = cornerTopRight = cornerBottomRight = false;
    trackpadStarted = false;//Prevents UpdateProperties method being executed during startup
    zoomDone = false;
    swipeDownDone = false;
    rotateDone = false;
    rotateMode = false;
    
    touchmode=MODE_MOVE;
    
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ElanTouchPad *
ApplePS2ElanTouchPad::probe( IOService * provider, SInt32 * score )
{
    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //
    _device  = (ApplePS2MouseDevice *) provider;
    bool Success = false;
    
    if (!super::probe(provider, score)) return 0;
    
    //Plz don't remove below line, I've worked really hard and deserve to be on this driver log
    IOLog("ElanTech Touchpad driver v1.7.0 by EMlyDinEsHMG (c) 2012\n");

    //Detecting the Presence of Elan Touchpad
    //
    IOLog("Elan: Detecting the touchpad\n");
    Elantech_detect(_device);
    //
    
    //Detecting Firmware Version
    /*
	 * Query touchpad's firmware version and see if it reports known
	 * value to avoid mis-detection. Logitech mice are known to respond
	 * to Elantech magic knock and there might be more.
	 */
    unsigned char param[3];
    
    if (Synaptics_send_cmd(_device, ETP_FW_VERSION_QUERY, param)) {
		IOLog("Elan :Failed to query firmware version.\n");
		return 0;
	}
    else
        Success = true;
    
	DEBUG_LOG("Elan: Elantech version query result 0x%02x, 0x%02x, 0x%02x.\n",
          param[0], param[1], param[2]);
    
	if (!(Elantech_is_signatures_valid(param))) {
		IOLog("Elan: Probably not a real Elantech touchpad. Aborting.\n");
		return 0;
	}
    fw_version = (param[0] << 16) | (param[1] << 8) | param[2];
    
    if (Elantech_set_properties(_device)) {
		IOLog("Elan: Unknown hardware version, aborting...\n");
		return 0;
	}
	IOLog("Elan: Touchpad found, Assuming hardware version %d (with firmware version 0x%02x%02x%02x)\n",
          hw_version, param[0], param[1], param[2]);
    //
    //
    
    if(hw_version == 3) //Only Supports V3 Hardware only, remaining I don't have hardware to test and implement it
    {
        if(Success)
            setProperty ("VersionNumber", 546, 32);
    }
    else
    {
        IOLog("Elan: Sorry your hardware versions %d is not supported yet by this driver, For help contact me on hem_dinesh@hotmail.com and I will try to support as much I can in my time but I can't promise\n",hw_version);
        Success = false;
    }
    
    if (Success) {
		OSDictionary *Configuration;
		Configuration = OSDynamicCast(OSDictionary, getProperty("Preferences"));
		if (Configuration){
            //IOLog("Entered Prefrences\n");
			OSString *tmpString = 0;
			OSNumber *tmpNumber = 0;
			OSData   *tmpData = 0;
			OSBoolean *tmpBoolean = false;
			OSData   *tmpObj = 0;
			bool tmpBool = false;
			UInt64 tmpUI64 = 0;
			
			OSIterator *iter = 0;
			const OSSymbol *dictKey = 0;
			
			iter = OSCollectionIterator::withCollection(Configuration);
			if (iter) {
				while ((dictKey = (const OSSymbol *)iter->getNextObject())) {
					tmpObj = 0;
					
					tmpString = OSDynamicCast(OSString, Configuration->getObject(dictKey));
					if (tmpString) {
						tmpObj = OSData::withBytes(tmpString->getCStringNoCopy(), tmpString->getLength()+1);
					}
					
					tmpNumber = OSDynamicCast(OSNumber, Configuration->getObject(dictKey));
					if (tmpNumber) {
						tmpUI64 = tmpNumber->unsigned64BitValue();
						tmpObj = OSData::withBytes(&tmpUI64, sizeof(UInt32));
					}
					
					tmpBoolean = OSDynamicCast(OSBoolean, Configuration->getObject(dictKey));
					if (tmpBoolean) {
						tmpBool = (bool)tmpBoolean->getValue();
						tmpObj = OSData::withBytes(&tmpBool, sizeof(bool));
                        
					}
					
					tmpData = OSDynamicCast(OSData, Configuration->getObject(dictKey));
					if (tmpData) {
						tmpObj = tmpData;
					}
					if (tmpObj) {
						//provider->setProperty(dictKey, tmpObj);
                        if(tmpUI64>0)
                            setProperty(dictKey->getCStringNoCopy(), tmpUI64 ,64);
                        else
                        setProperty(dictKey->getCStringNoCopy(), tmpBool?1:0 ,32);
                        
                        const char *tmpStr = dictKey->getCStringNoCopy();
                        
                        if(!strncmp(dictKey->getCStringNoCopy(),"TrackpadTappingCorners",strlen(tmpStr)))
                            _enableCornerTaps = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"EdgeScrolling",strlen(tmpStr)))
                            _enableEdgeCirular = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"SingleDoubleTapDrag",strlen(tmpStr)))
                            _singleDoubleTapDrag = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"PinchZoom",strlen(tmpStr)))
                            _enablePinchZoom = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"Rotation",strlen(tmpStr)))
                            _enableRotate = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFingerSwipeLeftRight",strlen(tmpStr)))
                            _enableSwipeLR = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFingerSwipeUpDown",strlen(tmpStr)))
                            _enableSwipeUpDwn = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"MouseButtonEnablesTouchpad",strlen(tmpStr)))
                            _mouseBtnsEnableTouch = tmpBool;
                        else if(!strncmp(dictKey->getCStringNoCopy(),"MousePointerDPI",strlen(tmpStr)))
                        {
                            _mouseDPI = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"AccidentalInputTimeOut", strlen(tmpStr)))
                        {
                            accInputTimeOut = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"CommandKeyPos", strlen(tmpStr)))
                        {
                            if(tmpUI64 == 0)
                                _device->dispatchPS2Notification(kCommandKeyPos_0);
                            else if(tmpUI64 == 1)
                                _device->dispatchPS2Notification(kCommandKeyPos_1);
                            else if(tmpUI64 == 2)
                                _device->dispatchPS2Notification(kCommandKeyPos_2);
                            else if(tmpUI64 == 3)
                                _device->dispatchPS2Notification(kCommandKeyPos_3);
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFingerSwipeUpAction", strlen(tmpStr)))
                        {
                            swipeUpAction = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFingerSwipeDownAction", strlen(tmpStr)))
                        {
                            swipeDownAction = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFIngerSwipeLeftAction", strlen(tmpStr)))
                        {
                            swipeLeftAction = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"ThreeFingerSwipeRightAction", strlen(tmpStr)))
                        {
                            swipeRightAction = tmpUI64;
                            tmpUI64 = 0;
                        }
                        else if(!strncmp(dictKey->getCStringNoCopy(),"KeyBoardNumLockOn",strlen(tmpStr)))
                        {
                            if(tmpBool)
                            _device->dispatchPS2Notification(kPS2C_NumLock);
                        }
                        
                        //OSSymbol *tmpConfSymb = OSDynamicCast(OSSymbol, getProperty("Preferences"));
                        //removeProperty(tmpConfSymb);
					}
				}
			}
		}
	}
    
    return (Success) ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ElanTouchPad::start( IOService * provider )
{
    //
    // The driver has been instructed to start. This is called after a
    // successful probe and match.
    //
    if (!super::start(provider)) return false;
    
    //
    // Maintain a pointer to and retain the provider object.
    //
    
    _device = (ApplePS2MouseDevice *) provider;
    _device->retain();
	
    
    //Querying the Capablities
    //
    if(send_cmd(_device, ETP_CAPABILITIES_QUERY,
                capabilities)) {
		IOLog("Elan: Failed to query capabilities.\n");
		return 0;
	}
	DEBUG_LOG("Elan: Synaptics capabilities query result 0x%02x, 0x%02x, 0x%02x.\n",
          capabilities[0], capabilities[1],
          capabilities[2]);
    //
    
    //Setting the Touchpad Properties and Mode
    //
    if (Elantech_set_absolute_mode(_device)) {
		IOLog("Elan: Failed to put the touchpad into absolute mode.\n");
		return false;
	}
    else
        IOLog("Elan: Successfully placed touchpad into Absolute mode\n");
    
	
    if(Elantech_set_range(_device, &_xmin, &_ymax, &_xmax, &_ymax, &width)) {
        IOLog("Elan: Failed to query touchpad range.\n");
        return false;
    }
    else
        DEBUG_LOG("Elan: Touchpad range X_min %d Y_min %d X_max %d Y_max %d\n", _xmin, _ymin, _xmax, _ymax);
    //
    
    
	pktsize = hw_version > 1 ? 6 : 4;
    DEBUG_LOG("Elan: Packet Size %d\n",pktsize);
    
    //
    // Write the TouchPad mode byte value.
    //
    //IOLog("Elan: TouchMode before %d\n",_touchPadModeByte);
    //setTouchPadModeByte(_touchPadModeByte);
    //
    //
    //Trying to enable the touchpad Tap if supported
    //setTapEnable(true);
    
    //SettingUp SampleRtae, Res
    setSampleRateAndResolution();
    
    getMouseInformation();
    
    // Must add this property to let our superclass know that it should handle
    // trackpad acceleration settings from user space.  Without this, tracking
    // speed adjustments from the mouse prefs panel have no effect.
    //
    
    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);
    setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDTrackpadScrollAccelerationKey);
    
	setProperty(kIOHIDScrollResolutionKey, (400 << 16), 32);
    setProperty(kIOHIDPointerResolutionKey, (_mouseDPI << 16), 32);
    setProperty(kIOHIDPointerButtonCountKey, 3, 32);
    
    //
    // Install our driver's interrupt handler, for asynchronous data delivery.
    //
    
    _device->installInterruptAction(this,OSMemberFunctionCast(PS2InterruptAction,this,&ApplePS2ElanTouchPad::interruptOccurred));
    
    _interruptHandlerInstalled = true;
    
    //
    // Enable the mouse clock (should already be so) and the mouse IRQ line.
    //
    
    setCommandByte( kCB_EnableMouseIRQ, kCB_DisableMouseClock );
    
    //
    // Finally, we enable the trackpad itself, so that it may start reporting
    // asynchronous events.
    //
    IOLog("Elan: Enabling the touchpad\n");
    Elantech_Touchpad_enable(true);
    
    
	// Install our power control handler.
	//
    
	_device->installPowerControlAction( this, OSMemberFunctionCast(PS2PowerControlAction,this,
                                                                   &ApplePS2ElanTouchPad::setDevicePowerState) );
    
    // Install our Keyboard Notification Handler
    //
    _device->installPS2NotificationAction(this, OSMemberFunctionCast(PS2NotificationAction, this, &ApplePS2ElanTouchPad::receiveKeyboardNotifications));
    
    _keyboardNotificationHandlerInstalled = true;
    
	_powerControlHandlerInstalled = true;
    
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::stop( IOService * provider )
{
    //
    // The driver has been instructed to stop.  Note that we must break all
    // connections to other service objects now (ie. no registered actions,
    // no pointers and retains to objects, etc), if any.
    //
    
    assert(_device == provider);
    
    //
    // Disable the mouse itself, so that it may stop reporting mouse events.
    //
    
    Elantech_Touchpad_enable(false);
    
    //
    // Disable the mouse clock and the mouse IRQ line.
    //
    
    setCommandByte( kCB_DisableMouseClock, kCB_EnableMouseIRQ );
    
    //
    // Uninstall the interrupt handler.
    //
    
    if ( _interruptHandlerInstalled )  _device->uninstallInterruptAction();
    _interruptHandlerInstalled = false;
    
    //
    // Uninstall the power control handler.
    //
    
    if ( _powerControlHandlerInstalled ) _device->uninstallPowerControlAction();
    _powerControlHandlerInstalled = false;
    
    // Uninstall the Keyboard Notification handler
    //
    if(_keyboardNotificationHandlerInstalled) _device->unistallPS2NotificationAction();
    _keyboardNotificationHandlerInstalled = false;
    
	super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::free()
{
    //
    // Release the pointer to the provider object.
    //
    
    if (_device)
    {
        _device->release();
        _device = 0;
    }
    
    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::interruptOccurred( UInt8 data )
{
    //
    // This will be invoked automatically from our device when asynchronous
    // events need to be delivered. Process the trackpad data. Do NOT issue
    // any BLOCKING commands to our device in this context.
    //
    // Ignore all bytes until we see the start of a packet, otherwise the
    // packets may get out of sequence and things will get very confusing.
    //
    /*
     * We check the constant bits to determine what packet type we get,
     * so packet checking is mandatory for v3 and later hardware.
     */
    const UInt8 debounce_packet[] = { 0xc4, 0xff, 0xff, 0x02, 0xff, 0xff };
    const UInt8 ack_packet[] = { 0x04 , 0x00, 0x00, 0x02, 0x00, 0x00  };
    const UInt8 lbtnDrag_packet[] = { 0x05 , 0x00, 0x00, 0x02, 0x00, 0x00  };
    AbsoluteTime now;

    
    //
    //Checking for the  accidental input based on the lastKeyPressTime
    //////////////////////
#if APPLESDK
    clock_get_uptime(&now);
#else
    clock_get_uptime((uint64_t*)&now);
#endif
    uint64_t accInputTimeDiff = (now - lastKeyPressTime);
    
    //IOLog("Elan: LastKeyPressTime %lld TimeOut %lld, Diff %lld \n",lastKeyPressTime,accInputTimeOut,accInputTimeDiff);
    if(accInputTimeDiff>accInputTimeOut || !_accidentalInput)
    {
        _accidentalInput = false;
        accidentalInputKeys = 0;
    }
    
    ////////////////////
    
    
    // Add this byte to the packet buffer. If the packet is complete, that is,
    // we have the 6 bytes, dispatch this packet for processing.
    //
    _packetBuffer[_packetByteCount++] = data;
    
    if (_packetByteCount == 6)
    {
        _packetByteCount = 0;
        unsigned char  *packet = _packetBuffer;
        
        switch (hw_version) {
            case 3:
                if (!memcmp(packet, lbtnDrag_packet, sizeof(lbtnDrag_packet)))
                {
                    lbtnDrag = true;
                }
                //If Acknoledgement Packet received then stop
                
                if (!memcmp(packet, ack_packet, sizeof(ack_packet)))
                {
                    //DEBUG_LOG("Elan: PS/2 ACK packet [ 0x%02x , 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                      //  packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
                    
                #if APPLESDK
                    clock_get_uptime(&now);
                #else
                    clock_get_uptime((uint64_t*)&now);
                #endif
                    
                    
                    //Sending Packet for double/tripe tap operations
                    if((touchmode == MODE_2_FING_TAP || touchmode == MODE_3_FING_TAP) && !fasterMode)
                        Elantech_report_absolute_v3(MODE_TAP , packet);
                    
                    if(fingersPressed || rotateCirLeft || rotateCirRight)
                        //Starting back the tracking after the end of the packet stream for the input
                        _StartTracking = true;
                    
                    track = 0;
                    fingers=0;
                    cameFrom = 0;
                    taps = 0;
                    
                    TwoFingerScroll = false;
                    circularStarted = false;
                    cornerTapped = false;
                    tempDisableCornerTaps = false;
                    zoomDone = zoomIn = zoomOut = false;
                    rotateDone = false;
                    rotateMode = false;
                    rotateCirRight = rotateCirLeft = false;
                    dirRight = dirLeft = false;
                    fingersPressed = false;

                    ScrollDelayCount = 0;
                    slowScrollDelay = 0;
                    holdDragDelayCount = 0;
                    lastPressure = -1;
                    pressure = -1;
                    lastTouchtime = 0;
                    curTouchtime = 0;
                    lastFingersNum = 0;
                    
                    touchmode = MODE_MOVE;
                    _ydiff = 0;
                    _xdiff = 0;
                    s_xdiff = 0;
                    s_ydiff = 0;
                    hscroll = 0;
                    vScroll = 0;
                    _startX1 = _startX2 = _startY1 = _startY2 = 0;
                    rotateXStart = rotateYStart = 0;
                    rotateXCounter = rotateYCounter = 0;
                    partialRotateR = partialRotateL = 0;

                    _initX = _initY = 0;
                    
                    
                    if(_singleDoubleTapDrag)
                    {
                        if(buttons == 0x0 && dragReleased && singleTapTouchTime<maxclicktime && singleTapTouchTime>0 && clicks == 1 && !dragged)//Single tap to Click Simulate
                        {
                            dispatchRelativePointerEvent(0, 0, 1, now);
                            dispatchRelativePointerEvent(0, 0, 0, now);
                            
                            clicks = 0;
                            singleTapTouchTime = 0;
                        }
                        else
                        {
                            clicks = 0;
                            singleTapTouchTime = 0;
                        }
                        
                    }
                    
                    if(buttons == 0x1 && dragReleased)
                    {
                        dispatchRelativePointerEvent(0, 0, 0, now);
                        buttons = 0x0;
                        buttonTriggered = false;
                        lbtnDrag = false;
                    }
                    
                    if(_draglock && !dragReleased)//Keeping drag value across stream of packets in case of tap to drag
                        dragging = true;
                    else
                    {
                        tapDragDelayCount = 0;//Clearing Drag Scroll vars
                        dragPressure = false;
                        dragStarted  = false;
                        lbtnDrag = false;
                        dragged = false;
                    }
                    
                    return;
                }
                /*
                 * check debounce first, it has the same signature in byte 0
                 * and byte 3 as PACKET_V3_HEAD.
                 */
                if (!memcmp(packet, debounce_packet, sizeof(debounce_packet)))
                {
                    //DEBUG_LOG("Elan: PS/2 Debounced packet [ 0x%02x , 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                    //  packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
                    return;
                }
                
                if (((packet[0] & 0x0c) == 0x04) && ((packet[3] & 0xcf) == 0x02) && _StartTracking)
                {
                    
                     //DEBUG_LOG("Elan: PS/2 Head packet [ 0x%02x , 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                     //packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
                    Elantech_report_absolute_v3(PACKET_V3_HEAD , packet);
                    return;
                }
                
                
                if ((packet[0] & 0x0c) == 0x0c && (packet[3] & 0xce) == 0x0c && _StartTracking)
                {
                    // DEBUG_LOG("Elan: PS/2 Tail packet [ 0x%02x , 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                    //packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
                    
                    Elantech_report_absolute_v3(PACKET_V3_TAIL , packet);
                    return;
                }
                //DEBUG_LOG("Elan: Unknown PS/2 packet [ 0x%02x , 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ]\n",
                // packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
                
                track = 1;//Tracking  value across the Stream of packets except at the first byte
                         //(meant for Initialization Operations) 
                
        
                break;
            case 4://////Not implemented Yet
                break;
            default:
                break;
        }
        
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::Elantech_Touchpad_enable(bool enable )
{
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //
    
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;
    
    // (mouse enable/disable command)
    //DEBUG_LOG("Elan: Enabling Mouse\n");
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = (enable)?kDP_Enable:kDP_SetDefaultsAndDisable;
    request->commandsCount = 1;
    _device->submitRequestAndBlock(request);
    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/////////Touchpad Detection method//////////
int ApplePS2ElanTouchPad::Elantech_detect(ApplePS2MouseDevice *device)
{
    unsigned char param[3];
    int i;
	
    parity[0] = 1;
	for (i = 1; i < 256; i++)
		this->parity[i] = this->parity[i & (i - 1)] ^ 1;
    
    PS2Request *          request = _device->allocateRequest();
    
    /*
     * Use magic knock to detect Elantech touchpad
     */
    // Disable stream mode before the command sequence
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetDefaultsAndDisable;
    
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = kDP_SetMouseScaling1To1;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = kDP_SetMouseScaling1To1;
    request->commands[3].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut  = kDP_SetMouseScaling1To1;
    //Reading Data
    request->commands[4].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut  = kDP_GetMouseInformation;
    request->commands[5].command = kPS2C_ReadDataPort;
    request->commands[5].inOrOut = 0;
    request->commands[6].command = kPS2C_ReadDataPort;
    request->commands[6].inOrOut = 0;
    request->commands[7].command = kPS2C_ReadDataPort;
    request->commands[7].inOrOut = 0;
    request->commandsCount = 8;
    
    device->submitRequestAndBlock(request);
    
    //Reading the Version details from the ports
    param[0] = request->commands[5].inOrOut;
    param[1] = request->commands[6].inOrOut;
    param[2] = request->commands[7].inOrOut;
    
    
    if(!(request->commandsCount == 8)) { //If Equals to Count then Success else its a failed request
        IOLog("Elan: sending Elantech magic knock failed.\n");
        device->freeRequest(request);
        return -1;
    }
    
    device->freeRequest(request);
    
    /*
	 * Report this in case there are Elantech models that use a different
	 * set of magic numbers
	 */
	if (param[0] != 0x3c || param[1] != 0x03 ||
	    (param[2] != 0xc8 && param[2] != 0x00)) {
		IOLog("Elan :Unexpected magic knock result 0x%02x, 0x%02x, 0x%02x.\n",
              param[0], param[1], param[2]);
		return -1;
	}
    
    return 0;
}

bool ApplePS2ElanTouchPad:: Elantech_is_signatures_valid(const unsigned char *param)
{
    static const unsigned char rates[] = { 200, 100, 80, 60, 40, 20, 10 };
	int i;
    
	if (param[0] == 0)
		return false;
    
	if (param[1] == 0)
		return true;
    
	for (i = 0; i < 7; i++)
		if (param[2] == rates[i])
			return false;
    
	return true;
}

///////////// Linux Style Slice command //////////
int ApplePS2ElanTouchPad::Elantech_Slice_command(ApplePS2MouseDevice *device,  unsigned char c)
{
    int i, cmdIndex =0;
    
    //Linux way of Sending Command
    PS2Request *request = _device->allocateRequest();
    
    //DEBUG_LOG("Elan : Slice Command Method\n");
    //Generic Style checking
    request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[cmdIndex].inOrOut  = kDP_SetMouseScaling1To1;
    cmdIndex++;
    
    
    //Synaptics Style Checking
    for (i = 6; i >= 0; i -= 2) {
		unsigned char d = (c >> i) & 3;
        //DEBUG_LOG("Elan: D:%d\n",(UInt8)d);
        request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[cmdIndex].inOrOut  = kDP_SetMouseResolution;
        cmdIndex++;
        request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[cmdIndex].inOrOut  = d;
        cmdIndex++;
	}
    
    
    request->commandsCount = cmdIndex;
    device->submitRequestAndBlock(request);
    
    
    if(!(request->commandsCount == cmdIndex)) {//If Equals then Success otherwise failed
        device->freeRequest(request);
        return -1;
    }
    device->freeRequest(request);
    
    
	return 0;
}

//////////// Synaptics style Commmand for Intercting with Device//////////
int ApplePS2ElanTouchPad::Synaptics_send_cmd(ApplePS2MouseDevice *device, unsigned char c, unsigned char *param)
{
    
    //DEBUG_LOG("Elan: Synaptics Send Command\n");
    int i, cmdIndex =0;
    PS2Request *request = device->allocateRequest();
    //Linux way of Sending Command
    
    //Generic Style checking
    request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[cmdIndex].inOrOut  = kDP_SetMouseScaling1To1;
    cmdIndex++;
    
    
    //Synaptics Style Checking
    for (i = 6; i >= 0; i -= 2) {
		unsigned char d = (c >> i) & 3;
        request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[cmdIndex].inOrOut  = kDP_SetMouseResolution;
        cmdIndex++;
        request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[cmdIndex].inOrOut  = d;
        cmdIndex++;
	}
    
    request->commands[cmdIndex].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[cmdIndex++].inOrOut  = kDP_GetMouseInformation;
    request->commands[cmdIndex].command = kPS2C_ReadDataPort;
    request->commands[cmdIndex++].inOrOut = 0;
    request->commands[cmdIndex].command = kPS2C_ReadDataPort;
    request->commands[cmdIndex++].inOrOut = 0;
    request->commands[cmdIndex].command = kPS2C_ReadDataPort;
    request->commands[cmdIndex++].inOrOut = 0;
    
    request->commandsCount = cmdIndex;
    device->submitRequestAndBlock(request);
    
    //Reading the Version details from the ports
    param[0] = request->commands[cmdIndex-3].inOrOut;
    param[1] = request->commands[cmdIndex-2].inOrOut;
    param[2] = request->commands[cmdIndex-1].inOrOut;
    
    if(!(request->commandsCount == cmdIndex)) {//If Equals then Success
        DEBUG_LOG("Elan :Elan: %s query 0x%02x failed.\n", __func__, c);
        device->freeRequest(request);
        return -1;
    }
    device->freeRequest(request);
    
    
	return 0;
}

///////////// Method to put the Touchpad Into Absolute Mode///////////
int ApplePS2ElanTouchPad::Elantech_set_absolute_mode(ApplePS2MouseDevice *device)
{
    unsigned char val = 0;
    int tries = ETP_READ_BACK_TRIES;
    int rc = 0;
    IOLog("Elan: Placing the Touchpad into Absolute mode\n");
    //
    // Reset the mouse as per Linux dump
    //
    
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return 0;
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_Reset;                          // 0xFF
    request->commandsCount = 1;
    _device->submitRequestAndBlock(request);
    _device->freeRequest(request);
    
    switch (hw_version) {
        case 1:
            reg_10 = 0x16;
            reg_11 = 0x8f;
            if (Elantech_write_reg(device, 0x10, &reg_10) ||
                Elantech_write_reg(device, 0x11, &reg_11)) {
                rc = -1;
            }
            break;
            
        case 2:
            /* Windows driver values */
            reg_10 = 0x54;
            reg_11 = 0x88;	/* 0x8a */
            reg_21 = 0x60;	/* 0x00 */
            if (Elantech_write_reg(device, 0x10, &reg_10) ||
                Elantech_write_reg(device, 0x11, &reg_11) ||
                Elantech_write_reg(device, 0x21, &reg_21)) {
                rc = -1;
            }
            break;
            
        case 3:
            reg_10 = 0x0b;
            if (Elantech_write_reg(device, 0x10, &reg_10))
                rc = -1;
            
            break;
            
        case 4:
            reg_07 = 0x01;
            if (Elantech_write_reg(device, 0x07, &reg_07))
                rc = -1;
            
            goto skip_readback_reg_10; /* v4 has no reg 0x10 to read */
    }
    
    if (rc == 0) {
        /*
         * Read back reg 0x10. For hardware version 1 we must make
         * sure the absolute mode bit is set. For hardware version 2
         * the touchpad is probably initializing and not ready until
         * we read back the value we just wrote.
         */
        do {
            rc = Elantech_read_reg(device, 0x10, &val);
            if (rc == 0)
                break;
            tries--;
            DEBUG_LOG("Elan:retrying read (%d).\n", tries);
            IOSleep(ETP_READ_BACK_DELAY);
        } while (tries > 0);
        
        if (rc) {
            IOLog("Elan: Failed to read back register 0x10.\n");
        }
        else if (hw_version == 1 && !(val & ETP_R10_ABSOLUTE_MODE)) {
            IOLog("Elan: touchpad refuses to switch to absolute mode.\n");
            rc = -1;
        }
    }
    
skip_readback_reg_10:
    if (rc)
        IOLog("Elan:Failed to initialise registers.\n");
    
    return rc;
    
}

/*
 * V3 and later support this fast command
 */
int ApplePS2ElanTouchPad::Elantech_send_cmd(ApplePS2MouseDevice *device, unsigned char c, unsigned char *param)
{
    //DEBUG_LOG("Elan: Elan Send Command\n");
    PS2Request *request = device->allocateRequest();
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = ETP_PS2_CUSTOM_COMMAND;
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = c;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = kDP_GetMouseInformation;
    request->commands[3].command = kPS2C_ReadDataPort;
    request->commands[3].inOrOut = 0;
    request->commands[4].command = kPS2C_ReadDataPort;
    request->commands[4].inOrOut = 0;
    request->commands[5].command = kPS2C_ReadDataPort;
    request->commands[5].inOrOut = 0;
    request->commandsCount = 6;
    device->submitRequestAndBlock(request);
    
    //Reading the Version details from the ports
    param[0] = request->commands[3].inOrOut;
    param[1] = request->commands[4].inOrOut;
    param[2] = request->commands[5].inOrOut;
    
    if(!(request->commandsCount == 6)) {//If Equals then Success
        IOLog("Elan : %s query 0x%02x failed.\n", __func__, c);
        device->freeRequest(request);
        return -1;
    }
    device->freeRequest(request);
    
    return 0;
}

/*
 * A retrying version of ps2_command
 */
int  ApplePS2ElanTouchPad::Elantech_ps2_cmd(ApplePS2MouseDevice *device, unsigned char *param, unsigned char c)
{
	int rc = -1;
	int tries = ETP_PS2_COMMAND_TRIES;
    //Linux way of Sending Command
    PS2Request *request = device->allocateRequest();
    
	do {
		
        //DEBUG_LOG("Elan : Elantech PS2 Command Method\n");
        //Generic Style checking
        request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
        request->commands[0].inOrOut  = c;
        request->commandsCount = 1;
        if(c == kDP_GetMouseInformation)
        {
            request->commands[1].command = kPS2C_ReadDataPort;
            request->commands[1].inOrOut = 0;
            request->commands[2].command = kPS2C_ReadDataPort;
            request->commands[2].inOrOut = 0;
            request->commands[3].command = kPS2C_ReadDataPort;
            request->commands[3].inOrOut = 0;
            request->commandsCount = 4;
        }
        
        device->submitRequestAndBlock(request);
        
        if(c == kDP_GetMouseInformation)
        {
            //Reading the Version details from the ports
            param[0] = request->commands[1].inOrOut;
            param[1] = request->commands[2].inOrOut;
            param[2] = request->commands[3].inOrOut;
            
            if(request->commandsCount == 4)//-1 indicates true which signifies failed and 0 is false for no errors
                rc = 0;
        }
        else
        {
            if(request->commandsCount == 1)//-1 indicates true which signifies error/failed and 0 is false for no errors
                rc = 0;
        }
        
        device->freeRequest(request);
        
		if (rc == 0)
			break;
		tries--;
		DEBUG_LOG("Elan: Retrying ps2 command 0x%02x (%d).\n",c, tries);
        
		IOSleep(ETP_PS2_COMMAND_DELAY);
        
	} while (tries > 0);
    
	if (rc)
		DEBUG_LOG("Elan: Ps2 command 0x%02x failed.\n", c);
    
	return rc;
}

int ApplePS2ElanTouchPad::Generic_ps2_cmd(ApplePS2MouseDevice *device, unsigned char *param, unsigned char c)
{
    int rc = -1;
    PS2Request *request = device->allocateRequest();
    
    //DEBUG_LOG("Elan : Generic PS2 Command Method\n");
    //Generic Style checking
    //request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    //request->commands[0].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = c;
    request->commandsCount = 1;
    if(c == kDP_GetMouseInformation)
    {
        request->commands[1].command = kPS2C_ReadDataPort;
        request->commands[1].inOrOut = 0;
        request->commands[2].command = kPS2C_ReadDataPort;
        request->commands[2].inOrOut = 0;
        request->commands[3].command = kPS2C_ReadDataPort;
        request->commands[3].inOrOut = 0;
        request->commandsCount = 4;
    }
    
    device->submitRequestAndBlock(request);
    
    if(c == kDP_GetMouseInformation)
    {
        //Reading the Version details from the ports
        param[0] = request->commands[1].inOrOut;
        param[1] = request->commands[2].inOrOut;
        param[2] = request->commands[3].inOrOut;
        
        if(request->commandsCount == 4)//-1 indicates true which signifies error/failed
            rc = 0;
    }
    else
    {
        if(request->commandsCount == 1)//-1 indicates true which signifies error/failed
            rc = 0;
    }
    
    
    device->freeRequest(request);
    return rc;
}

/*
 * determine hardware version and set some properties according to it.
 */
int ApplePS2ElanTouchPad::Elantech_set_properties(ApplePS2MouseDevice *device)
{
    //DEBUG_LOG("Elan: Elan Set Properties\n");
    /* This represents the version of IC body. */
	int ver = (fw_version & 0x0f0000) >> 16;
    DEBUG_LOG("Elan: FirmWare Version %d\n",fw_version);
    
    
	/* Early version of Elan touchpads doesn't obey the rule. */
	if (fw_version < 0x020030 || fw_version == 0x020600)
		hw_version = 1;
	else {
		switch (ver) {
            case 2:
            case 4:
                hw_version = 2;
                break;
            case 5:
                hw_version = 3;
                break;
            case 6:
                hw_version = 4;
                break;
            default:
                return -1;
		}
	}
    
	/* decide which send_cmd we're gonna use early */
	send_cmd =  hw_version >= 3 ? Elantech_send_cmd :
    Synaptics_send_cmd;
    
	/* Turn on packet checking by default */
	paritycheck = 1;
    
	/*
	 * This firmware suffers from misreporting coordinates when
	 * a touch action starts causing the mouse cursor or scrolled page
	 * to jump. Enable a workaround.
	 */
	jumpy_cursor =
    (fw_version == 0x020022 || fw_version == 0x020600);
    
	if (hw_version > 1) {
		/* For now show extra debug information */
		debug = 1;
        
		if (fw_version >= 0x020800)
			reports_pressure = true;
	}
    
    return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn ApplePS2ElanTouchPad::setParamProperties( OSDictionary * config )
{
    //OSNumber * tapdrag = OSDynamicCast( OSNumber, config->getObject("SinglgeTapDrag") );
    //OSNumber * edgeScroll = OSDynamicCast( OSNumber, config->getObject("EdgeScrolling") );
    //OSNumber * corners = OSDynamicCast( OSNumber, config->getObject("TrackpadTappingCorners") );
    OSNumber * jitter = OSDynamicCast( OSNumber, config->getObject("JitterNoMove") );
    OSNumber * btnswap  = OSDynamicCast( OSNumber, config->getObject("TrackpadRightClick") );
    OSNumber * clicking = OSDynamicCast( OSNumber, config->getObject("Clicking") );
	OSNumber * dragging = OSDynamicCast( OSNumber, config->getObject("Dragging") );
	OSNumber * draglock = OSDynamicCast( OSNumber, config->getObject("DragLock") );
    OSNumber * hscroll  = OSDynamicCast( OSNumber, config->getObject("TrackpadHorizScroll") );
    OSNumber * vscroll  = OSDynamicCast( OSNumber, config->getObject("TrackpadScroll") );
    OSNumber * eaccell  = OSDynamicCast( OSNumber, config->getObject("HIDTrackpadScrollAcceleration") );
	OSNumber * accell   = OSDynamicCast( OSNumber, config->getObject("HIDTrackpadAcceleration") );
    OSNumber * maxclicktiming   = OSDynamicCast( OSNumber, config->getObject("HIDClickTime") );
    
	//DEBUG_LOG(" Elan: Setting ParamProperties\n");
	//config->removeObject("HIDPointerAcceleration");
    
    //_edgeaccell values from prefane : 0 2 8 11 14 16 24 32 40 48
    if (eaccell)
    {
        _edgeaccell = eaccell->unsigned32BitValue();
        _edgeaccellvalue = (((double)(_edgeaccell / 1966.08)) / 375.0);// just for ref, we are not using it
        _edgeaccellvalue = _edgeaccellvalue == 0 ? 0.01 : _edgeaccellvalue;// just for ref, we are not using it
        setProperty("HIDTrackpadScrollAcceleration", eaccell);
    }
    if (accell) {
		setProperty("HIDTrackpadAcceleration", accell->unsigned32BitValue());
	}
    if (dragging)
	{
		_dragging = dragging->unsigned32BitValue() & 0x1 ? true : false;
		setProperty("Dragging", dragging);
	}
    
	if (draglock)
	{
		_draglock = draglock->unsigned32BitValue() & 0x1 ? true : false;
		setProperty("DragLock", draglock);
	}
    
    if (hscroll)
    {
        _edgehscroll = hscroll->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("TrackpadHorizScroll", hscroll);
    }
    
    if (vscroll)
    {
        _edgevscroll = (vscroll->unsigned32BitValue() & 0x1) ? true : false;
        setProperty("TrackpadScroll", vscroll);
    }
    if(maxclicktiming)
    {
        _maxclicktiming = maxclicktiming->unsigned64BitValue();
        setProperty ("HIDClickTime", maxclicktime, 64);
    }
    if(clicking)
    {
        _clicking = (clicking->unsigned32BitValue()&0x1)?true:false;
        setProperty ("Clicking", _clicking?1:0, 32);
    }
    if (btnswap)
    {
        _buttonSwap = btnswap->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("TrackpadRightClick", btnswap);
    }
    if(jitter)
    {
        _jitter = (jitter->unsigned32BitValue()&0x1)?true:false;
        setProperty ("JitterNoMove", _jitter?1:0, 32);
        //_disableTouch = _jitter;
        _enableTypingMode = _jitter;
    }
    /*if(tapdrag)
     {
     _singleDoubleTapDrag = (tapdrag->unsigned32BitValue()&0x1)?true:false;
     setProperty ("SingleDoubleTapDrag", _singleDoubleTapDrag?1:0, 32);
     }
     if(edgeScroll)
     {
     _enableEdgeCirular = (edgeScroll->unsigned32BitValue()&0x1)?true:false;
     setProperty ("SupportsGestureScrolling", _enableEdgeCirular?1:0, 32);
     }
     if(corners)
     {
     _enableCornerTaps = (corners->unsigned32BitValue()&0x1)?true:false;
     
     setProperty("TrackpadTappingCorners",_enableCornerTaps?1:0,32);
     
     }*/
    
    return super::setParamProperties(config);
}

IOReturn ApplePS2ElanTouchPad::setProperties (OSObject *props)
{
	OSDictionary *pdict;
    
    if ((pdict=OSDynamicCast (OSDictionary, props)))
        return setParamProperties (pdict);
    
	return kIOReturnError;
}

bool ApplePS2ElanTouchPad::updateProperties()
{
    if(trackpadStarted)
    {
        //IOLog("Elan: Updating Properties\n");
        
        setProperty("TrackpadTappingCorners",_enableCornerTaps?1:0,32);
        
        setProperty("TrackpadRightClick", _buttonSwap?1:0,32);
        
        setProperty ("EdgeScrolling", _enableEdgeCirular?1:0, 32);
        
        setProperty ("PinchZoom", _enablePinchZoom?1:0, 32);
        
        setProperty ("Rotation", _enableRotate?1:0, 32);

        setProperty ("ThreeFingerSwipeLeftRight", _enableSwipeLR?1:0, 32);

        setProperty ("ThreeFingerSwipeUpDown", _enableSwipeUpDwn?1:0, 32);

        setProperty ("MouseButtonEnablesTouchpad", _mouseBtnsEnableTouch?1:0, 32);
        
    }
    return super::updateProperties();
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -



////////// Method to set the TouchPad Range values //////////////
int ApplePS2ElanTouchPad::Elantech_set_range(ApplePS2MouseDevice *device,
                                             unsigned int *x_min, unsigned int *y_min,
                                             unsigned int *x_max, unsigned int *y_max,
                                             unsigned int *width)
{
    unsigned char param[3];
	unsigned char traces;
    
    DEBUG_LOG("Elan: Setting touchpad Range\n");
	switch (hw_version) {
        case 1:
            *x_min = ETP_XMIN_V1;
            *y_min = ETP_YMIN_V1;
            *x_max = ETP_XMAX_V1;
            *y_max = ETP_YMAX_V1;
            break;
            
        case 2:
            if (fw_version == 0x020800 ||
                fw_version == 0x020b00 ||
                fw_version == 0x020030) {
                *x_min = ETP_XMIN_V2;
                *y_min = ETP_YMIN_V2;
                *x_max = ETP_XMAX_V2;
                *y_max = ETP_YMAX_V2;
            } else {
                int i;
                int fixed_dpi;
                
                i = (fw_version > 0x020800 &&
                     fw_version < 0x020900) ? 1 : 2;
                
                if (send_cmd(device, ETP_FW_ID_QUERY, param))
                    return -1;
                
                fixed_dpi = param[1] & 0x10;
                
                if (((fw_version >> 16) == 0x14) && fixed_dpi) {
                    if (send_cmd(device, ETP_SAMPLE_QUERY, param))
                        return -1;
                    
                    *x_max = (capabilities[1] - i) * param[1] / 2;
                    *y_max = (capabilities[2] - i) * param[2] / 2;
                } else if (fw_version == 0x040216) {
                    *x_max = 819;
                    *y_max = 405;
                } else if (fw_version == 0x040219 || fw_version == 0x040215) {
                    *x_max = 900;
                    *y_max = 500;
                } else {
                    *x_max = (capabilities[1] - i) * 64;
                    *y_max = (capabilities[2] - i) * 64;
                }
            }
            break;
            
        case 3:
            if (send_cmd(device, ETP_FW_ID_QUERY, param))
                return -1;
            *x_max = (0x0f & param[0]) << 8 | param[1];
            *y_max = (0xf0 & param[0]) << 4 | param[2];
            break;
            
        case 4:
            if (send_cmd(device, ETP_FW_ID_QUERY, param))
                return -1;
            
            *x_max = (0x0f & param[0]) << 8 | param[1];
            *y_max = (0xf0 & param[0]) << 4 | param[2];
            traces = capabilities[1];
            if ((traces < 2) || (traces > *x_max))
                return -1;
            
            *width = *x_max / (traces - 1);
            DEBUG_LOG("Elan: Touchpad Pad CentreX %d, CentreY %d, Width %d\n",centerx,centery,*width);
            break;
	}
    //Setting Up Touchpad Centre's
    centerx = *x_max/2;
    centery = *y_max/2;
    
    DEBUG_LOG("Elan: Touchpad Pad CentreX %d, CentreY %d\n",centerx,centery);
    
    
    return  0;
}
/*
 * Send an Elantech style special command to write a register with a value
 */
int ApplePS2ElanTouchPad::Elantech_write_reg(ApplePS2MouseDevice *device, unsigned char reg, unsigned char *val)
{
    
	int rc = 0;
    
	if (reg < 0x07 || reg > 0x26)
		return -1;
    
	if (reg > 0x11 && reg < 0x20)
		return -1;
    //DEBUG_LOG("Elan: HW %d\n",hw_version);
	switch (hw_version) {
        case 1:
            if (Elantech_Slice_command(device, ETP_REGISTER_WRITE) ||
                Elantech_Slice_command(device, reg) ||
                Elantech_Slice_command(device, *val) ||
                Generic_ps2_cmd(device, NULL, kDP_SetMouseScaling1To1)) {
                rc = -1;
            }
            break;
            
        case 2:
            if (Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, ETP_REGISTER_WRITE) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, reg) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, *val) ||
                Elantech_ps2_cmd(device, NULL, kDP_SetMouseScaling1To1)) {
                rc = -1;
            }
            break;
            
        case 3:
            if (Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, ETP_REGISTER_READWRITE) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, reg) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, *val) ||
                Elantech_ps2_cmd(device, NULL, kDP_SetMouseScaling1To1)) {
                rc = -1;
            }
            break;
            
        case 4:
            if (Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, ETP_REGISTER_READWRITE) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, reg) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, ETP_REGISTER_READWRITE) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, *val) ||
                Elantech_ps2_cmd(device, NULL, kDP_SetMouseScaling1To1)) {
                rc = -1;
            }
            break;
	}
    
	if (rc)
		IOLog("Elan: Failed to write register 0x%02x with value 0x%02x.\n", reg, *val);
    
	return rc;
    
}

/*
 * Send an Elantech style special command to read a value from a register
 */
int ApplePS2ElanTouchPad::Elantech_read_reg(ApplePS2MouseDevice *device, unsigned char reg, unsigned char *val)
{
    
	unsigned char param[3];
	int rc = 0;
    
	if (reg < 0x07 || reg > 0x26)
		return -1;
    
	if (reg > 0x11 && reg < 0x20)
		return -1;
    
	switch (hw_version) {
        case 1:
            if (Elantech_Slice_command(device, ETP_REGISTER_READ) ||
                Elantech_Slice_command(device, reg) ||
                Generic_ps2_cmd(device, param, kDP_GetMouseInformation)) {
                rc = -1;
            }
            break;
            
        case 2:
            if (Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device,  NULL, ETP_REGISTER_READ) ||
                Elantech_ps2_cmd(device,  NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device,  NULL, reg) ||
                Elantech_ps2_cmd(device, param, kDP_GetMouseInformation)) {
                rc = -1;
            }
            break;
            
        case 3 ... 4:
            if (Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, ETP_REGISTER_READWRITE) ||
                Elantech_ps2_cmd(device, NULL, ETP_PS2_CUSTOM_COMMAND) ||
                Elantech_ps2_cmd(device, NULL, reg) ||
                Elantech_ps2_cmd(device, param, kDP_GetMouseInformation)) {
                rc = -1;
            }
            break;
	}
    
	if (rc)
		IOLog("Elan: Failed to read register 0x%02x.\n", reg);
	else if (hw_version != 4)
		*val = param[0];
	else
		*val = param[1];
    
	return rc;
    
}

/*
 * (value from firmware) * 10 + 790 = dpi
 * we also have to convert dpi to dots/mm (*10/254 to avoid floating point) for V4
 */
unsigned int ApplePS2ElanTouchPad::Elantech_convert_res(unsigned int val)
{
	return (val * 10 + 790) * 10 / 254;
}

int ApplePS2ElanTouchPad::Elantech_get_resolution_v4(ApplePS2MouseDevice *device, unsigned int *x_res, unsigned int *y_res)
{
	unsigned char param[3];
    
	if (Elantech_send_cmd(device, ETP_RESOLUTION_QUERY, param))
		return -1;
    
	*x_res = Elantech_convert_res(param[1] & 0x0f);
	*y_res = Elantech_convert_res((param[1] & 0xf0) >> 4);
    
	return 0;
}

/*
 * REPORT PACKETS
 */
/*void ApplePS2ElanTouchPad::Elantech_report_absolute_v1(ApplePS2MouseDevice *device)
 {
 unsigned char *packet = psmouse->packet;
 int fingers;
 
 if (etd->fw_version < 0x020000) {
 /*
 * byte 0:  D   U  p1  p2   1  p3   R   L
 * byte 1:  f   0  th  tw  x9  x8  y9  y8
 *
 fingers = ((packet[1] & 0x80) >> 7) +
 ((packet[1] & 0x30) >> 4);
 } else {
 /*
 * byte 0: n1  n0  p2  p1   1  p3   R   L
 * byte 1:  0   0   0   0  x9  x8  y9  y8
 *
 fingers = (packet[0] & 0xc0) >> 6;
 }
 
 if (etd->jumpy_cursor) {
 if (fingers != 1) {
 etd->single_finger_reports = 0;
 } else if (etd->single_finger_reports < 2) {
 /* Discard first 2 reports of one finger, bogus *
 etd->single_finger_reports++;
 elantech_debug("discarding packet\n");
 return;
 }
 }
 
 input_report_key(dev, BTN_TOUCH, fingers != 0);
 
 /*
 * byte 2: x7  x6  x5  x4  x3  x2  x1  x0
 * byte 3: y7  y6  y5  y4  y3  y2  y1  y0
 *
 if (fingers) {
 input_report_abs(dev, ABS_X,
 ((packet[1] & 0x0c) << 6) | packet[2]);
 input_report_abs(dev, ABS_Y,
 etd->y_max - (((packet[1] & 0x03) << 8) | packet[3]));
 }
 
 input_report_key(dev, BTN_TOOL_FINGER, fingers == 1);
 input_report_key(dev, BTN_TOOL_DOUBLETAP, fingers == 2);
 input_report_key(dev, BTN_TOOL_TRIPLETAP, fingers == 3);
 input_report_key(dev, BTN_LEFT, packet[0] & 0x01);
 input_report_key(dev, BTN_RIGHT, packet[0] & 0x02);
 
 if (etd->fw_version < 0x020000 &&
 (etd->capabilities[0] & ETP_CAP_HAS_ROCKER)) {
 /* rocker up *
 input_report_key(dev, BTN_FORWARD, packet[0] & 0x40);
 /* rocker down *
 input_report_key(dev, BTN_BACK, packet[0] & 0x80);
 }
 
 input_sync(dev);
 }*/

/*void ApplePS2ElanTouchPad::Elantech_report_absolute_v2(ApplePS2MouseDevice *device)
 {
 unsigned char *packet = psmouse->packet;
 unsigned int fingers, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
 unsigned int width = 0, pres = 0;
 
 /* byte 0: n1  n0   .   .   .   .   R   L *
 fingers = (packet[0] & 0xc0) >> 6;
 
 switch (fingers) {
 case 3:
 /*
 * Same as one finger, except report of more than 3 fingers:
 * byte 3:  n4  .   w1  w0   .   .   .   .
 *
 if (packet[3] & 0x80)
 fingers = 4;
 /* pass through... *
 case 1:
 /*
 * byte 1:  .   .   .   .  x11 x10 x9  x8
 * byte 2: x7  x6  x5  x4  x4  x2  x1  x0
 *
 x1 = ((packet[1] & 0x0f) << 8) | packet[2];
 /*
 * byte 4:  .   .   .   .  y11 y10 y9  y8
 * byte 5: y7  y6  y5  y4  y3  y2  y1  y0
 *
 y1 = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
 
 pres = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
 width = ((packet[0] & 0x30) >> 2) | ((packet[3] & 0x30) >> 4);
 break;
 
 case 2:
 /*
 * The coordinate of each finger is reported separately
 * with a lower resolution for two finger touches:
 * byte 0:  .   .  ay8 ax8  .   .   .   .
 * byte 1: ax7 ax6 ax5 ax4 ax3 ax2 ax1 ax0
 *
 x1 = (((packet[0] & 0x10) << 4) | packet[1]) << 2;
 /* byte 2: ay7 ay6 ay5 ay4 ay3 ay2 ay1 ay0 *
 y1 = etd->y_max -
 ((((packet[0] & 0x20) << 3) | packet[2]) << 2);
 /*
 * byte 3:  .   .  by8 bx8  .   .   .   .
 * byte 4: bx7 bx6 bx5 bx4 bx3 bx2 bx1 bx0
 *
 x2 = (((packet[3] & 0x10) << 4) | packet[4]) << 2;
 /* byte 5: by7 by8 by5 by4 by3 by2 by1 by0 *
 y2 = etd->y_max -
 ((((packet[3] & 0x20) << 3) | packet[5]) << 2);
 
 /* Unknown so just report sensible values *
 pres = 127;
 width = 7;
 break;
 }
 
 input_report_key(dev, BTN_TOUCH, fingers != 0);
 if (fingers != 0) {
 input_report_abs(dev, ABS_X, x1);
 input_report_abs(dev, ABS_Y, y1);
 }
 elantech_report_semi_mt_data(dev, fingers, x1, y1, x2, y2);
 input_report_key(dev, BTN_TOOL_FINGER, fingers == 1);
 input_report_key(dev, BTN_TOOL_DOUBLETAP, fingers == 2);
 input_report_key(dev, BTN_TOOL_TRIPLETAP, fingers == 3);
 input_report_key(dev, BTN_TOOL_QUADTAP, fingers == 4);
 input_report_key(dev, BTN_LEFT, packet[0] & 0x01);
 input_report_key(dev, BTN_RIGHT, packet[0] & 0x02);
 if (etd->reports_pressure) {
 input_report_abs(dev, ABS_PRESSURE, pres);
 input_report_abs(dev, ABS_TOOL_WIDTH, width);
 }
 
 input_sync(dev);
 
 }*/


void ApplePS2ElanTouchPad::Elantech_report_absolute_v3(int packet_type, unsigned char *packets)
{
    unsigned char *packet = packets;
	unsigned x1 = 0, y1 = 0, x2 = 0, y2 = 0, temp = 0;
    
    AbsoluteTime now;
    
    //Data fingers, width and Pressure from the packets
    pressure = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
	width = ((packet[0] & 0x30) >> 2) | ((packet[3] & 0x30) >> 4);
    /* byte 0: n1  n0   .   .   .   .   R   L */
	fingers = (packet[0] & 0xc0) >> 6;
    
    
    //SettingUp Two fingers Rotate Mode for avoiding conflicts with Two Finger Zoom fucntions when arc rotating
   if(lastFingersNum == 1 && fingers == 2 && ScrollDelayCount>25)
     {
         rotateMode = true;
         rotateDone = false;
         track = 0;
         lastFingersNum = 0;
     }
    else if(fingers == 1)
        lastFingersNum = 1;

    
    //Hard Mouse Buttons Detection
    if ( packet[0] == 0x5)
    {
        buttons = 0x1;  // buttons |= 0x1; left button   (bit 0 set in packet)
        touchmode = MODE_BUTTONS;
    }
    else if ( packet[0] == 0x6 )
    {
        buttons = 0x2;  // right button  (bit 1 set in packet)
        touchmode = MODE_BUTTONS;
    }
    else if ( packet[0] == 0x7 )
    {
        buttons = 0x4;  // middle button  (bit 0 & 1 set in packet)
        touchmode = MODE_BUTTONS;
    }
   //
    
    #if APPLESDK
        clock_get_uptime(&now);
    #else
        clock_get_uptime((uint64_t*)&now);
    #endif
    
    DEBUG_LOG("Elan: Pressure %d Width %d, Fingers %d ScrollDelay %d\n", pressure, width, fingers,ScrollDelayCount);
    
    
    
    if(track != 0)
        curTouchtime += (*(uint64_t*)&now)-lastTouchtime;
    
    lastTouchtime = (*(uint64_t*)&now);
    
    //////////Packet byte Steam with Pressure 0//////////
    if(pressure == 0 ||
       rotateCirLeft || rotateCirRight ||
       fingersPressed)
        //Pressure 0 Packet Bytes stream is received at the end of the stream for every input in V3
        //I'm using this Pressure 0 reported packet stream for invoking the functions
        //to the respective gesture
    {
        Process_End_functions(packet_type, packets);
    }
    
    //////////// Single and Three Fingers Touch Tracking//////////////
    else if(((fingers == 1  && pressure<120) ||
              (fingers == 3 && pressure > 100 && width > 7)) &&
              !_disableTouch && !_accidentalInput)
    {
        /*
         * byte 1:  .   .   .   .  x11 x10 x9  x8
         * byte 2: x7  x6  x5  x4  x4  x2  x1  x0
         */
        x1 = ((packet[1] & 0x0f) << 8) | packet[2];
        /*
         * byte 4:  .   .   .   .  y11 y10 y9  y8
         * byte 5: y7  y6  y5  y4  y3  y2  y1  y0
         */
        //y1 = y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
        temp = (((packet[4] & 0x0f) << 8) | packet[5]);
        y1 = _ymax - temp;
        
        lastCordinatesX = x1;
        lastCordinatesY = y1;

        if(fingers == 3)
            Process_Threefingers_touch(x1, y1);
        else if(fingers == 1 && !rotateDone)
            Process_singlefinger_touch(x1, y1);
        
    }
    ////////////////// Two Fingers Touch Tracking/////////////////////
    else if(fingers == 2 && !_disableTouch && !threeFingerMode)
                                            //Preventing accidental two finger touches duing three finger
    {
                
        if (packet_type == PACKET_V3_HEAD) {
            /*
             * byte 1:   .    .    .    .  ax11 ax10 ax9  ax8
             * byte 2: ax7  ax6  ax5  ax4  ax3  ax2  ax1  ax0
             */
            lastCordinatesX = ((packet[1] & 0x0f) << 8) | packet[2];
            /*
             * byte 4:   .    .    .    .  ay11 ay10 ay9  ay8
             * byte 5: ay7  ay6  ay5  ay4  ay3  ay2  ay1  ay0
             */
            lastCordinatesY = _ymax -
            (((packet[4] & 0x0f) << 8) | packet[5]);
            /*
             * wait for next packet
             */
            return;
        }
        x1=lastCordinatesX;
        y1=lastCordinatesY;
        /* packet_type == PACKET_V3_TAIL */
        x2 = ((packet[1] & 0x0f) << 8) | packet[2];
        y2 = _ymax - (((packet[4] & 0x0f) << 8) | packet[5]);
        
        Process_twofingers_touch(x1, x2, y1, y2);
        
    }
    
    return;
    
}

void ApplePS2ElanTouchPad::Process_End_functions(int packet_type, unsigned char *packets)
{
    /******Absolute Pointer Vars***/
    IOGPoint boundsABS = { static_cast<SInt16>(_xrest), static_cast<SInt16>(_yrest) };
    
    IOGBounds boundsPAD = { 0, static_cast<SInt16>(_xmax), 0, static_cast<SInt16>(_ymax)};
    
    unsigned char *packet = packets;
    
                
        /***************Circular Rotate and Pinch Zoom Simulation***************/
        
        if(rotateCirLeft && _enableRotate)
        {
            _device->dispatchPS2Notification(kPS2C_RotateLeft);
            touchmode = MODE_ROTATE;
        }
        else if(rotateCirRight && _enableRotate)
        {
            _device->dispatchPS2Notification(kPS2C_RotateRight);
            touchmode = MODE_ROTATE;
        }
        else if(zoomOut && _enablePinchZoom)
        {
            if((zoomDone && (zoomXDiff>800 || zoomYDiff>800)) || (!zoomDone && zoomXDiff<800 && zoomYDiff<800))
                _device->dispatchPS2Notification(kPS2C_ZoomPlus);
            else if(zoomXDiff>800 || zoomYDiff>800)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomPlus);
                _device->dispatchPS2Notification(kPS2C_ZoomPlus);
            }
            touchmode = MODE_ZOOM;
        }
        else if(zoomIn && _enablePinchZoom)
        {
            if((zoomDone && (zoomXDiff>800 || zoomYDiff>800)) || (!zoomDone && zoomXDiff<800 && zoomYDiff<800))
                _device->dispatchPS2Notification(kPS2C_ZoomMinus);
            else if(zoomXDiff>800 || zoomYDiff>800)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomMinus);
                _device->dispatchPS2Notification(kPS2C_ZoomMinus);
            }
            touchmode = MODE_ZOOM;
        }
    
    //Resetting for Immediate Pinch zoom to work
    zoomIn = zoomOut  = false;
    //////// Stopping processing the packet stream becoz we have generated the output so no need to process
    if(rotateCirLeft || rotateCirRight || fingersPressed)
        _StartTracking = false;
        
        AbsoluteTime now;
        
        #if APPLESDK
            clock_get_uptime(&now);
        #else
            clock_get_uptime((uint64_t*)&now);
        #endif
    
    
    /********** Corner tapping Toggle **********/
    if(cornerTapped)
    {
        
        boundsABS.x = _xmax/2;
        boundsABS.y = _ymax/2;
        
        dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, buttons, true, 0, 30, 160, 0, now);
        
        //CleanUp and reset
        curTouchtime = 0;
        lastTouchtime = 0;
        track = 0;
        ScrollDelayCount = 0;
        holdDragDelayCount = 0;
        
        touchmode = MODE_MOVE;//Setting this mode as default
        
        cornerTapped = cornerTopLeft = cornerBottomLeft = cornerTopRight = cornerBottomRight = false;

    }
    /**********Single Tap/Clicking/Hard Buttons/Drag Start**************/
    
    else if(curTouchtime <= maxclicktime && pressure<120 &&
           (touchmode == MODE_MOVE || touchmode == MODE_BUTTONS) && ScrollDelayCount<8 &&
           (!_accidentalInput || touchmode == MODE_BUTTONS))
        {
            DEBUG_LOG("Elan: CurTouchTime %lld, MaxClickTime %lld Buttons %d Pressure %d, Delay %d, Tap %d\n",curTouchtime, maxclicktime,buttons,pressure,ScrollDelayCount,tapDragDelayCount);
            
            if(swipeDownDone)//Releasing the Command Key on Tab+Command after swipe down
            {
                _device->dispatchPS2Notification(kPS2C_ReleaseKey);
                swipeDownDone = false;
            }
            
            
            if(buttons == 0x1 && _draglock  && !dragReleased && !(touchmode == MODE_BUTTONS))
                //Releasing the Drag lock on tap
            {
                dispatchRelativePointerEvent(0, 0, 0x1, now);
                dispatchRelativePointerEvent(0, 0, 0x0, now);
                
                dragReleased = true;
                dragging = false;
                buttons = 0x0;
                dragPressure = false;
                tapDragDelayCount = 0;
                lbtnDrag = false;
                dragged = true;//For avoiding the text selection from clearing on tap release
                
            }
            else if((buttons == 0x1 || buttons == 0x4|| buttons == 0x2) &&
                    touchmode == MODE_BUTTONS) {//Simulating the Hard Buttons click functions
                
                if(buttons == 0x1 && packet[1] == 0x0)
                    //If We hold the left button then packet 1 will contain 0x0 on second stream
                {
                    lbtnDrag = true;
                    dispatchRelativePointerEvent(0, 0, 1, now);
                }
                else if(buttons == 0x2 && packet[1] == 0x0)
                {
                    
                    //Disabling the touchpad on Mouse Right button double click
                    if(_mouseBtnsEnableTouch)
                    {
                        _disableTouch = !_disableTouch;
                        if(_disableTouch)
                            IOLog("Elan: Touchpad is disabled\n");
                        else
                            IOLog("Elan: Touchpad is enabled\n");
                        
                        dispatchRelativePointerEvent(0, 25, 0, now);//For Showing setting change with pointer movement
                        buttons = 0x0;//Release, don't sent the button
                    }
                    
                }
                else if(buttons == 0x4 && packet[1] == 0x0)
                {
                    _enableEdgeCirular = !_enableEdgeCirular;
                    if(_enableEdgeCirular)
                        IOLog("Elan: Edge Scrolling(With Circular) Enabled\n");
                    else
                        IOLog("Elan: Edge Scrolling(With Circular) Disabled\n");
                    
                    dispatchRelativePointerEvent(-25, 0, 0, now);
                    buttons = 0x0;//Release, don't send the button
                    
                    trackpadStarted = true;
                    updateProperties();
                    
                }
                else if(buttons == 0x4)//Preventing conflicts from button 1 & 2 when pressing button holding button 4
                    midBtn = true;
                
                
            }
            else if((buttons == 0x0 && dragReleased) || buttons == 0x1 || buttons == 0x4|| buttons == 0x2)
                //Single Tap to Click and Hard Buttons Simulation
            {
                
                if(_singleDoubleTapDrag)
                {
                    //For Working double tap to move the App on Title bar
                    //but Single Tap to Click response will be little slower
                    if(curTouchtime>0)
                        singleTapTouchTime = curTouchtime;
                    
                    clicks++;
                    
                    //for quick response on double/triple clicking operations in this mode
                    if(clicks ==2 || clicks == 3)
                    {
                        for(int i=0;i<clicks;i++)
                        {
                            dispatchRelativePointerEvent(0, 0, 0x1, now);
                            dispatchRelativePointerEvent(0, 0, 0x0, now);
                        }
                        tapDragDelayCount = 0;
                    }
                    else if(clicks>3)
                    {
                        dispatchRelativePointerEvent(0, 0, 0x1, now);
                        dispatchRelativePointerEvent(0, 0, 0x0, now);
                        tapDragDelayCount = 0;
                    }
                }
                else if(buttons == 0x0 && !buttonTriggered)
                {
                    dispatchRelativePointerEvent(0, 0, 0x1, now);
                    dispatchRelativePointerEvent(0, 0, 0x0, now);//For next click to work we have to release last click
                    tapDragDelayCount = 0;
                    lbtnDrag = false;
                    buttonTriggered = false;
                    midBtn = false;
                    buttons = 0x0;
                    
                    if(rbtn) {//Clearing the Context click menu  after a left click function
                        rbtn = false;
                        buttonTriggered = false;
                    }
                }
                
                //Hard buttons
                if((buttons == 0x1 && !midBtn) || buttonTriggered)
                    //In this way, the double tap to drag on title bar won't work
                    //but tap to click will be normal
                {
                    if(buttons == 0x0)
                        dispatchRelativePointerEvent(0, 0, 0x1, now);
                    else
                    dispatchRelativePointerEvent(0, 0, buttons, now);
                    
                    dispatchRelativePointerEvent(0, 0, 0x0, now);//For next click to work we have to release last click
                    
                    tapDragDelayCount = 0;
                    lbtnDrag = false;
                    buttonTriggered = false;
                    midBtn = false;
                    buttons = 0x0;
                    
                    if(_accidentalInput && _mouseBtnsEnableTouch)
                        //Enabling the touchpad to work when left mouse button is pressed in accidental input mode
                        //before the timeout
                        _accidentalInput = false;
                    
                    if(rbtn) {//Clearing the Context click menu  after a left click function
                        rbtn = false;
                        buttonTriggered = false;
                    }
                    
                }
                else if((buttons == 0x2 && !midBtn)|| (buttons == 0x4 || midBtn))
                {
                    if(!midBtn)
                    {
                        dispatchRelativePointerEvent(0, 0, 0x2, now);

                        rbtn = true;
                    }
                    else if(midBtn)
                    {
                        dispatchRelativePointerEvent(0, 0, 0x4, now);
                        dispatchRelativePointerEvent(0, 0, 0, now);
                        
                    }
                    
                    buttonTriggered = true;
                }
                
            }
            
            
            if(dragging && !_draglock)
                IODelay(300);//For dragging timeout
            
            if(!dragging)
                dragging = true; // dragging will work if it receives another tap
            //otherwise gets cleared at second release/ACK
            
            //CleanUp and reset
            taps = 0;
            track = 0;
            lastTouchtime = 0;
            curTouchtime = 0;
            ScrollDelayCount = 0;
            holdDragDelayCount = 0;
            touchmode = MODE_MOVE;//Setting this mode as default
            
        }
        /******** Three Fingers Swiping****************/
    
        else if(touchmode == MODE_MUL_TOUCH)
        {
            if(swipeLeftDone)
            {
                if(swipeLeftAction == 0)
                    _device->dispatchPS2Notification(kPS2C_SwipeLeft);
                else if(swipeLeftAction == 1)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_1);
                else if(swipeLeftAction == 2)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_2);
                else if(swipeLeftAction == 3)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_3);
                else if(swipeLeftAction == 4)//Notification centre
                {
                    boundsABS.x = _xmax-50;
                    boundsABS.y = 25;
                    
                    dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, 0x1, true, 0, 30, 160, 0, now);
                    
                }
                else if(swipeLeftAction == 5)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_4);
                else if(swipeLeftAction == 6)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_5);
                else if(swipeLeftAction == 7)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_6);

            }
            else if(swipeRightDone)
            {
                
                    if(swipeRightAction == 0)
                        _device->dispatchPS2Notification(kPS2C_SwipeRight);
                    else if(swipeRightAction == 1)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_1);
                    else if(swipeRightAction == 2)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_2);
                    else if(swipeRightAction == 3)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_3);
                    else if(swipeRightAction == 4)//Notification centre
                    {
                        boundsABS.x = _xmax-50;
                        boundsABS.y = 25;
                        
                        dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, 0x1, true, 0, 30, 160, 0, now);
                        
                    }
                    else if(swipeRightAction == 5)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_4);
                    else if(swipeRightAction == 6)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_5);
                    else if(swipeRightAction == 7)
                        _device->dispatchPS2Notification(kPS2C_SwipeAction_6);
            }
            else if(swipeDownDone)
            {
                if(swipeDownAction == 0 || swipeDownAction == 1)
                    _device->dispatchPS2Notification(kPS2C_SwipeDown);
                else if(swipeDownAction == 2)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_2);
                else if(swipeDownAction == 3)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_3);
                else if(swipeDownAction == 4)//Notification centre
                {
                    boundsABS.x = _xmax-50;
                    boundsABS.y = 25;
                    
                    dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, 0x1, true, 0, 30, 160, 0, now);

                }
                else if(swipeDownAction == 5)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_4);
                else if(swipeDownAction == 6)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_5);
                else if(swipeDownAction == 7)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_6);

            }
            else if(swipeUpDone)
            {
                if(swipeUpAction == 0 || swipeUpAction == 2)
                    _device->dispatchPS2Notification(kPS2C_SwipeUp);
                else if(swipeUpAction == 1)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_2);
                else if(swipeUpAction == 3)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_3);
                else if(swipeUpAction == 4)//Notification center
                {
                    boundsABS.x = _xmax-50;
                    boundsABS.y = 25;
                    
                    dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, 0x1, true, 0, 30, 160, 0, now);
                    
                }
                else if(swipeUpAction == 5)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_4);
                else if(swipeUpAction == 6)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_5);
                else if(swipeUpAction == 7)
                    _device->dispatchPS2Notification(kPS2C_SwipeAction_6);
            }
            
            //CleanUp and reset
            lastTouchtime = 0;
            curTouchtime = 0;
            track = 0;
            ScrollDelayCount = 0;
            swipeLeftDone = swipeRightDone = swipeUpDone = false;

        }
        /******** Three Fingers Tapping****************/
    
        else if(touchmode == MODE_3_FING_TAP &&  curTouchtime <= maxtaptime && ScrollDelayCount<9)
        {
            
            DEBUG_LOG("Elan: CurTouchTime %lld, MaxTapTime_ThreeFinger %lld, Taps %d\n",curTouchtime, maxtaptime,taps);
            
            taps++;
            
            if(((taps == 2 && packet_type == MODE_TAP) ||
                (taps == 1  && fasterMode))
               && _clicking)//Middle Button/Contextual Menu triggering
            {
            if(_buttonSwap)
                    buttons = 0x4;
                else
                    buttons = 0x2;
                
                if(buttonTriggered && (buttons == 0x2 || buttons == 0x4))
                {
                    dispatchRelativePointerEvent(0, 0, buttons, now);
                    dispatchRelativePointerEvent(0, 0, 0, now);

                    rbtn = false;
                    buttonTriggered = false;
                    midBtn = false;
                }
                else
                {
                    dispatchRelativePointerEvent(0, 0, buttons, now);
                    
                    buttonTriggered = true;
                    if(buttons == 0x2)
                        rbtn = true;
                    else if(buttons == 0x4)
                    {
                        dispatchRelativePointerEvent(0, 0, 0, now);
                        midBtn = true;
                    }
                    
                    taps = 0;
                }
                
            }
            else if(taps == 3 && packet_type == MODE_TAP  && !fasterMode)
                //Taps are 2 but additonal one value is for the detection in the last packet
            {
                _enablePinchZoom = !_enablePinchZoom;
                
                if(_enablePinchZoom)
                    IOLog("Elan: Pinch Zooming enabled\n");
                else
                    IOLog("Elan: Pinch Zooming disabled\n");
                
                dispatchRelativePointerEvent(0, -25, 0, now);
                
                trackpadStarted = true;
                updateProperties();
                taps = 0;
            }
            else if(taps == 4 && packet_type == MODE_TAP  && !fasterMode)
            {
                _enableRotate = !_enableRotate;
                if(_enableRotate)
                    IOLog("Elan: Rotations enabled\n");
                else
                    IOLog("Elan: Rotations disabled\n");
                
                dispatchRelativePointerEvent(0, 25, 0, now);
                
                trackpadStarted = true;
                updateProperties();
                taps = 0;
            }
            
            //CleanUp and reset
            lastTouchtime = 0;
            curTouchtime = 0;
            track = 0;
            ScrollDelayCount = 0;
            
        }
        
        /********** Two Fingers Tapping ****/
        else if((touchmode == MODE_2_FING_TAP) && curTouchtime <= maxtaptime && ScrollDelayCount<9)
        {
            DEBUG_LOG("Elan: CurTouchTime %lld, MaxTapTime_TwoFinger %lld, Taps %d\n",curTouchtime, maxtaptime,taps);
            
            taps++;
            
            
            if(((taps == 2 && packet_type == MODE_TAP) ||
                (taps == 1  && fasterMode))
               && _clicking)//Middle Button/Contextual Menu triggering
            {
                if(_buttonSwap)
                    buttons = 0x2;
                else
                    buttons = 0x4;
                
                
                if(buttonTriggered && (buttons == 0x2 || buttons == 0x4))
                {
                    dispatchRelativePointerEvent(0, 0, buttons, now);
                    dispatchRelativePointerEvent(0, 0, 0, now);
                    
                    rbtn = false;
                    buttonTriggered = false;
                    midBtn = false;
                }
                else
                {
                    dispatchRelativePointerEvent(0, 0, buttons, now);

                    buttonTriggered = true;
                    if(buttons == 0x2)
                        rbtn = true;
                    else if(buttons == 0x4)
                    {
                        dispatchRelativePointerEvent(0, 0, 0, now);
                        midBtn = true;
                    }
                    
                    taps = 0;
                }
                
            }
            else if(taps == 3 && packet_type == MODE_TAP && !fasterMode)
            {
                _enableEdgeCirular = !_enableEdgeCirular;//Disabling the Cirular and Edge Scrolling on double tap
                if(_enableEdgeCirular)
                    IOLog("Elan: Edge Scrolling(With Circular) Enabled\n");
                else
                    IOLog("Elan: Edge Scrolling(With Circular) Disabled\n");
                
                dispatchRelativePointerEvent(-25, 0, 0, now);
                
                
                trackpadStarted = true;
                updateProperties();
                taps = 0;
            }
            else if(taps == 4 && packet_type == MODE_TAP && !fasterMode)
            {
                _enableCornerTaps = !_enableCornerTaps; //Disabling corner taps
                
                if(_enableCornerTaps)
                    IOLog("Elan: Corner Tappping is Enabled\n");
                else
                    IOLog("Elan: Corner Tappping is Disabled\n");
                
                dispatchRelativePointerEvent(25, 0, 0, now);
                
                trackpadStarted = true;
                updateProperties();
                taps = 0;
            }

            //CleanUp and reset
            lastTouchtime = 0;
            curTouchtime = 0;
            track = 0;
            ScrollDelayCount = 0;
            
        }
    
        //CleanUp and reset
        if(curTouchtime > maxclicktime && ScrollDelayCount>=9) {
            curTouchtime = 0;
            lastTouchtime = 0;
            track = 0;
            ScrollDelayCount = 0;
            holdDragDelayCount = 0;
            taps = 0;
            
            touchmode = MODE_MOVE;//Setting this mode as default
            dispatchScrollWheelEvent(0, 0, 0, now);
            dispatchRelativePointerEvent(0, 0, buttons, now);
        }
    
    //Additional must CleanUp at the Pressure 0 Stream 
        swipeLeftDone = swipeRightDone = swipeUpDone = false;
        threeFingerMode = false;
        TwoFingerScroll = false;
    
    }


void ApplePS2ElanTouchPad::Process_Threefingers_touch(int midfinger_x, int midfinger_y)
{

        int tmp_xDiff =0,  tmp_yDiff = 0;
        int xdiff = 0, ydiff = 0;
    
    
    if(!threeFingerMode)//For the first time we Intialize
    {
        _lastX = _lastY = 0;
        _initX = midfinger_x;
        _initY = midfinger_y;
        threeFingerMode = true;
    }
    
    //Differneces in the Touch coordinates used for the tracking the position
    xdiff = (midfinger_x - _lastX);
    ydiff = (midfinger_y - _lastY);

        if(ScrollDelayCount<7)
            touchmode = MODE_3_FING_TAP;
        
        ScrollDelayCount++;//Using same variable ScrollDelayCount which also used in two finger for counting the three finger time on touchpad
        
        tmp_xDiff = _initX - midfinger_x;
        tmp_yDiff = _initY - midfinger_y;
        
        if(tmp_xDiff<0)
            tmp_xDiff = 0 - tmp_xDiff;
        if(tmp_yDiff<0)
            tmp_yDiff = 0 - tmp_yDiff;
    
    
        AbsoluteTime now;
        
        #if APPLESDK
            clock_get_uptime(&now);
        #else
            clock_get_uptime((uint64_t*)&now);
        #endif

        _lastX = midfinger_x;
        _lastY = midfinger_y;
    
    
    DEBUG_LOG("Elan: Three Finger Touch X1 %d Y1 %d xdiff %d ydiff %d,TmpXD %d, TmpYD %d, pressure %d, width %d, track %d Scroll %d\n",midfinger_x,midfinger_y,xdiff,ydiff,tmp_xDiff,tmp_yDiff,pressure,width,track,ScrollDelayCount);

        if(ScrollDelayCount>35 && tmp_xDiff<25 && tmp_yDiff<25)
        {
            touchmode = MODE_THREE_FING_PRESS;
            
            fasterMode =!fasterMode;
            if(fasterMode)
                IOLog("Elan: Faster mode enabled, so two and three finger double/triple tapping will not work\n");
            else
                IOLog("Elan: Faster mode disabled, so two and three finger double/triple tapping will work\n");
            
            dispatchRelativePointerEvent(0, 25, 0, now);//For Showing setting change with pointer movement
            
            fingersPressed =true;
        }
        
        else if((_initX - midfinger_x)> 0 && tmp_xDiff>tmp_yDiff && tmp_xDiff>50 && width>7 && pressure>100
                && ScrollDelayCount>7 && !track == 0 && _enableSwipeLR && taps == 0
                && touchmode != MODE_THREE_FING_PRESS)
        {
            swipeLeftDone = true;
            swipeDownDone = swipeUpDone = swipeRightDone = false;
            touchmode = MODE_MUL_TOUCH;
            return;
        }
        else if((_initX - midfinger_x)< 0 && tmp_xDiff>tmp_yDiff && tmp_xDiff>50  && width>7 && pressure>100
                && ScrollDelayCount>7 && !track == 0 && _enableSwipeLR && taps == 0
                && touchmode != MODE_THREE_FING_PRESS)
        {
            swipeRightDone = true;
            swipeDownDone = swipeLeftDone = swipeUpDone = false;
            touchmode = MODE_MUL_TOUCH;
            return;
        }
        else if( (_initY - midfinger_y)< 0 && tmp_yDiff>tmp_xDiff && tmp_yDiff>50 && width>7 && pressure>100
                && ScrollDelayCount>7 && !track == 0 && _enableSwipeUpDwn && taps == 0
                && touchmode != MODE_THREE_FING_PRESS)
        {
            swipeDownDone = true;
            swipeUpDone = swipeLeftDone = swipeRightDone = false;
            touchmode = MODE_MUL_TOUCH;
            return;
        }
        else if((_initY - midfinger_y)> 0 && tmp_yDiff>tmp_xDiff && tmp_yDiff>50 && width>7 && pressure>100
                && ScrollDelayCount>7 && !track == 0 && _enableSwipeUpDwn && taps == 0
                && touchmode != MODE_THREE_FING_PRESS)
        {
            swipeUpDone = true;
            swipeDownDone = swipeLeftDone = swipeRightDone = false;
            touchmode = MODE_MUL_TOUCH;//Just for Preventing other modes from taking over
            return;
        }
        
        track = 1;
        
    return;
}


void ApplePS2ElanTouchPad::Process_singlefinger_touch(int x, int y)
{
    int xdiff = 0, ydiff = 0, scrollFactor =0, e_xdiff = 0, e_ydiff = 0;
    int tempxd = 0, tempyd = 0, maxUnXJmp = 0;
    /******Absolute Pointer Vars***/
    IOGPoint boundsABS = { static_cast<SInt16>(_xrest), static_cast<SInt16>(_yrest) };
    
    IOGBounds boundsPAD = { 0, static_cast<SInt16>(_xmax), 0, static_cast<SInt16>(_ymax)};

    
    if(track == 0)
    {
        _lastX = _lastY = 0;
        _initX = x;
        _initY = y;
    }
    
    //Differneces in the Touch coordinates used for the tracking the position
    xdiff = (x -_lastX);
    ydiff = (y - _lastY);

    
    
    DEBUG_LOG("Elan: Single Finger X1 %d Y1 %d xdiff %d ydiff %d, pressure %d, width %d, track %d, Buttons %d, ScrollDelay %d\n",x,y,xdiff,ydiff,pressure,width,track, buttons,ScrollDelayCount);
 
    AbsoluteTime now;
    
    #if APPLESDK
        clock_get_uptime(&now);
    #else
        clock_get_uptime((uint64_t*)&now);
    #endif

    
        if((touchmode == MODE_EDGE_VSCROLL && x<2300) || touchmode == MODE_CIR_VSCROLL)
            touchmode = MODE_CIR_VSCROLL;
        else if((touchmode == MODE_EDGE_HSCROLL && y<1200) || touchmode == MODE_CIR_HSCROLL)
            touchmode = MODE_CIR_HSCROLL;
        else if(x>_xmax-150 && y>120 && y<_ymax-120 && _enableEdgeCirular)
            touchmode = MODE_EDGE_VSCROLL;
        else if(x >200 && x<_xmax-150 && y>_ymax-120 && _enableEdgeCirular)
            touchmode = MODE_EDGE_HSCROLL;
        
        
        if(_enableCornerTaps)//Corner Tapping rejection during Edge scrolling
        {
            
            if((touchmode == MODE_CIR_HSCROLL || touchmode == MODE_CIR_VSCROLL || touchmode == MODE_EDGE_HSCROLL || touchmode == MODE_EDGE_VSCROLL))
                tempDisableCornerTaps = true;
            else
                tempDisableCornerTaps = false;
        }
        else
            tempDisableCornerTaps = false;
    
    
    //////CORNER'S TAPPING//////////
    //Top Left Corner
    if(x<150 && y<150 && _enableCornerTaps && pressure<60 && !_disableTouch && !tempDisableCornerTaps  && ScrollDelayCount<10)//Preventing corner tap during normal mouse movements
    {
        boundsABS.x = 0;
        boundsABS.y = 0;
        if(track == 0)
            dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, buttons, true, 0, 30, 160, 0, now);
        else
            dispatchRelativePointerEvent(-150, -150, buttons, now);
        cornerTapped = true;
        cornerTopLeft = true;
    }
    else if(x<150 && (y>_ymax-150) && _enableCornerTaps && pressure<60 && !_disableTouch && !tempDisableCornerTaps && ScrollDelayCount<10)//Botton Left Corner
    {
        boundsABS.x = 0;
        boundsABS.y = _ymax;
        
        if(track == 0)
            dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, buttons, true, 0, 30, 160, 0, now);
        else
            dispatchRelativePointerEvent(-150, 150, buttons, now);
        cornerTapped = true;
        cornerBottomLeft = true;
    }
    else if((x>_xmax-150) && y<150 && _enableCornerTaps && pressure<60 && !_disableTouch && !tempDisableCornerTaps
            && ScrollDelayCount<10)//Top Right Corner
    {
        boundsABS.x = _xmax;
        boundsABS.y = 0;
        if(track == 0)
            dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, buttons, true, 0, 30, 160, 0, now);
        else
            dispatchRelativePointerEvent(150, -150, buttons, now);
        cornerTapped = true;
        cornerTopRight = true;
    }
    else if((x>_xmax-150) && (y>_ymax-150) && _enableCornerTaps && pressure<60 && !_disableTouch && !tempDisableCornerTaps && ScrollDelayCount<10)//Bottom Right Corner
    {
        boundsABS.x = _xmax;
        boundsABS.y = _ymax;
        if(track == 0)
            dispatchAbsolutePointerEvent(&boundsABS, &boundsPAD, buttons, true, 0, 30, 160, 0, now);
        else
            dispatchRelativePointerEvent(150, 150, buttons, now);
        cornerTapped = true;
        cornerBottomRight = true;
    }
        else if(!cornerTapped)
        {
            switch (touchmode) {///////Switching modes in Single Touch
                case MODE_BUTTONS://For dragging with left button
                case MODE_HSCROLL://For Dual finger switching during pointing
                case MODE_VSCROLL://For Dual Finger Only
                case MODE_2_FING_TAP://For Dual Finger Only
                case MODE_TWO_FINGERS_PRESS://For Dual Finger Only
                case MODE_MOVE:
                    
                    
                    //tapDragDelayCounts enable us to escape the dragging from quick mouse movements to some extent
                    if((dragging && _dragging && _draglock && !rbtn  && tapDragDelayCount>10 && dragPressure) || (_draglock && (lbtnDrag || touchmode == MODE_BUTTONS)))
                    {
                        buttons= 0x1;
                        dragReleased = false;
                        tapDragDelayCount = 11;//Stopping from getting big values
                        
                    }
                    else if((dragging && _dragging && !_draglock && !rbtn && tapDragDelayCount>10 && dragPressure) || (!_draglock && (lbtnDrag || touchmode == MODE_BUTTONS)))
                    {
                        buttons= 0x1;
                        dragReleased = true;
                        tapDragDelayCount = 11;//Stopping from getting big values
                        
                    }
                    else if(rbtn)
                        buttons = 0x2;
                    else if(!dragging && holdDragDelayCount>20)//Drag on Single Finger hold
                        buttons = 0x1;
                    else
                        buttons = 0x0;
                    
                    
                    //Unexpected jumps fix
                    if(_xdiff<0 && xdiff>0)
                        tempxd = xdiff - _xdiff;
                    else if(_xdiff>0 && xdiff<0)
                        tempxd = _xdiff - xdiff;
                    else
                        tempxd = _xdiff - xdiff;
                    
                    if(_ydiff<0 && ydiff>0)
                        tempyd = ydiff - _ydiff;
                    else if(_ydiff>0 && ydiff<0)
                        tempyd = _ydiff - ydiff;
                    else
                        tempyd = _ydiff - ydiff;
                    
                    //IOLog("Elan: TempXD %d TempYD %d\n",tempxd,tempyd);
                    
                    if((_xdiff<6 && _xdiff>-6) || (_ydiff<6 && _ydiff>-6))
                        maxUnXJmp = 20;
                    else if((_xdiff<20 && _xdiff>-20) || (_ydiff<20 && _ydiff>-20))
                        maxUnXJmp = 45;
                    else
                        maxUnXJmp = 80;
                    //End fix
                    
                    if(track == 0 || TwoFingerScroll || (tempyd>=maxUnXJmp && _ydiff<50 && _ydiff>-50) || (_xdiff<50 && _xdiff>-50 && tempxd>=maxUnXJmp))
                    {
                        dispatchRelativePointerEvent(0, 0, buttons, now);
                        TwoFingerScroll = false;
                        _xdiff = _ydiff = 0;//Resetting on Unexpected Jumps
                    }
                    else
                    {
                        //dispatchRelativePointerEvent(0, 0, buttons, now);
                        dispatchRelativePointerEvent(xdiff/divisor, ydiff/divisor , buttons, now);                        
                    }
                    
                    
                    /*if(track == 0)
                     dispatchRelativePointerEvent(0, 0, buttons, now);
                     else
                     dispatchAbsolutePointerEvent(&boundsABS,
                     &boundsPAD,
                     buttons,
                     false,
                     30,
                     30,
                     160,
                     NULL,
                     now);*/
                    
                    touchmode = MODE_MOVE;
                    
                    break;
                    
                    
                case MODE_EDGE_VSCROLL:
                    if(track == 0)
                        _lastY = 0;
                    
                    e_ydiff = y - _lastY;
                    
                    if (e_ydiff > 0)
                        e_ydiff = -1;
                    else if (e_ydiff < 0)
                        e_ydiff = 1;
                    
                    
                    scrollFactor = (int)((int)_edgeaccell/(256*16));
                    
                    dispatchScrollWheelEvent((e_ydiff*scrollFactor), 0, 0, now);
                    
                    break;
                    
                case MODE_EDGE_HSCROLL:
                    
                    if(track == 0)
                        _lastX = 0;
                    
                    e_xdiff = x - _lastX;
                    
                    if (e_xdiff > 0)
                        e_xdiff = -1;
                    else if (e_xdiff < 0)
                        e_xdiff = 1;
                    
                    scrollFactor = (int)((int)_edgeaccell/(256*16));
                    
                    dispatchScrollWheelEvent(0, (e_xdiff*scrollFactor), 0, now);
                    
                    
                    break;
                    
                case MODE_CIR_VSCROLL:
                    if(_enableEdgeCirular)
                    {
                        if(track == 0)
                            _lastY = 0;
                        int y_diff = y - _lastY, x_diff = x-_lastX, scroll;
                        
                        
                        if ((y_diff >= 0 && x_diff < -5 && y>centery && x>centerx) ||
                            (y_diff <= 0 && x_diff < -5 && y>centery && x<centerx) ||
                            (y_diff <= 0 && x_diff > 5 && y<centery && x<centerx) ||
                            (y_diff >= 0 && x_diff > 5 && y<centery && x>centerx))
                            scroll = -1;
                        else if ((y_diff <= 0 && x_diff < -5 && x>centerx && y<centery) ||
                                 (y_diff >= 0 && x_diff < -5 && x<centerx && y<centery) ||
                                 (y_diff >= 0 && x_diff > 5 && x<centerx && y>centery) ||
                                 (y_diff <= 0 && x_diff > 5 && x>centerx && y>centery))
                            scroll = 1;
                        else
                            scroll = 0;
                        
                        
                        int scrollFactor = (int)((int)_edgeaccell/(256*16));  //Value from Trackpad.prefpanes
                        
                        if(scroll != 0)
                            dispatchScrollWheelEvent((scroll*scrollFactor), 0, 0, now);
                        
                        
                        _lastX = x;
                        _lastY = y;
                        track = 1;
                        
                        
                    }
                    break;
                    
                case MODE_CIR_HSCROLL:
                    if(_enableEdgeCirular)
                    {
                        if(track == 0)
                            _lastY = 0;
                        int y_diff = y - _lastY, x_diff = x-_lastX, scroll;
                        
                        if ((y_diff >= 0 && x_diff < -5 && y>centery && x>centerx) ||
                            (y_diff >= 0 && x_diff > 5 && y<centery && x>centerx) ||
                            (y_diff <= 0 && x_diff > 5 && y<centery && x<centerx) ||
                            (y_diff <= 0 && x_diff < -5 && y>centery && x<centerx))
                            scroll = -1;
                        else if((y_diff >= 0 && x_diff > 5 && x<centerx && y>centery) ||
                                (y_diff >= 0 && x_diff < -5 && x<centerx && y<centery) ||
                                (y_diff <= 0 && x_diff < -5 && x>centerx && y<centery) ||
                                (y_diff <= 0 && x_diff > 5 && x>centerx && y>centery))
                            scroll = 1;
                        else
                            scroll = 0;
                        
                        
                        int scrollFactor = (int)((int)_edgeaccell/(256*16));  //Value from Trackpad.prefpanes
                        
                        if(scroll != 0)
                            dispatchScrollWheelEvent(0, (scroll*scrollFactor), 0, now);
                        
                        _lastX = x;
                        _lastY = y;
                        track = 1;
                        
                    }
                    break;
                    
                default:
                    
                    break;
            }
            
            
        }
    
    track = 1;

        _lastX = x;
        _lastY = y;
        
        _xdiff = xdiff;
        _ydiff = ydiff;
        
        if(track == 0)
            _xdiff = _ydiff = 0;
        
        
        
        lastPressure = pressure;// For Drag Lock release
        if(pressure>60)// Setting Drag to Start if pressure is above 60
            dragPressure = true;
        
        //Incrementing for Tap detection
        ScrollDelayCount++;//For Escaping with Singel finger touch during movement and for drag lock/tap
        //Using same var ScrollDelayCount used in two finger for counting the Single finger time on touchpad here
        
        
        //Timeout for Single finger holdDrag to start
        if((xdiff<3 && xdiff>-3 && ydiff<3 && ydiff>-3) && !dragging && pressure>65 && ScrollDelayCount<35 && x<(_initX+15) && x>(_initX-15) && y<(_initY+25) && y>(_initY-25))
            holdDragDelayCount++;
        
        //Timeout for Single finger tapDrag to start
        if((xdiff<5 && xdiff>-5 && ydiff<5 && ydiff>-5) && pressure>50 && ScrollDelayCount<15 && x<(_initX+15) && x>(_initX-15) && y<(_initY+25) && y>(_initY-25))
            tapDragDelayCount++;
        
        if(ScrollDelayCount>35)
            ScrollDelayCount = 35; //Keeping this constans after 35 which is our required maximum value for conditional
        // preventing from getting higher values and stopping dragging to occur on normal mouse movements in redetection
    
    return;
    
}

void ApplePS2ElanTouchPad::Process_twofingers_touch(int x1, int x2, int y1, int y2)
{
 
    /*Two Finger Scrolling Modes
     *
     *****/
    int xdiff_2 = 0, ydiff_2 = 0, scrollFactor = 0, xdiff_1 = 0, ydiff_1 = 0;
    int x1_Diff = 0, y1_Diff = 0, x2_Diff = 0, y2_Diff = 0;

    TwoFingerScroll = true;
    
    //scrollFactor values with below formula from trackpad pane : 0 2 8 11 14 16 24 32 40 48
    scrollFactor =  (int)((int)_edgeaccell/(256*16));
    
    
    if(track == 0 || _startX1 ==  0)
    {
        _lastX1 = _lastY1 = _lastX2 = _lastY2 = 0;
        _xdiff_2 = _ydiff_2 = _xdiff_1 = _ydiff_1 = 0;
        _startX1 = x1;
        _startX2 = x2;
        _startY1 = y1;
        _startY2 = y2;
        
        if((_startX1-_startX2)<0)
            zoomXDiff = (_startX2 - _startX1) ;
        else
            zoomXDiff = (_startX1 - _startX2);
        if((_startY1-_startY2)<0)
            zoomYDiff = (_startY2 - _startY1);
        else
            zoomYDiff = (_startY1 - _startY2);
    }
    
    xdiff_2 = x1 - _lastX1;
    ydiff_2 = y1 - _lastY1;
    //Other Finger
    //xdiff_1 = x2 - _lastX2;
    //ydiff_1 = y2 - _lastY2;
    
    if(_startX2 == 0 && _startY2 == 0)
    {
        _startX2 = x2;
        _startY2 = y2;
    }
    
    x1_Diff = _startX1 - x1;
    y1_Diff = _startY1 - y1;
    x2_Diff = _startX2 - x2;
    y2_Diff = _startY2 - y2;
    
    int tempxd_2 = 0, tempyd_2 = 0, maxUnXJmp = 0, lastmode = 0;
    
    //s_xdiff = 0; //reset
    //s_ydiff = 0; //reset
    
    zoomXDiff = (_startX1 - _startX2)  - (x1 -x2);;
    if(zoomXDiff<0)
        zoomXDiff = 0 - zoomXDiff;
    zoomYDiff = (_startY1 - _startY2) - (y1 - y2);
    if(zoomYDiff<0)
        zoomYDiff = 0 - zoomYDiff;
    
    AbsoluteTime now;
    
    #if APPLESDK
        clock_get_uptime(&now);
    #else
        clock_get_uptime((uint64_t*)&now);
    #endif

    
    DEBUG_LOG("Elan: X Diff %d, Y Diff %d, XD1 %d, YD1 %d, XD2 %d, YD2 %d,X1 %d, X2 %d, Y1 %d, Y2 %d track %d, ZoomXDIff %d, ZoomYDiff %d, Scroll %d, R %d, RD %d\n",(x1-x2),(y1-y2),(_startX1-x1), (_startY1-y1), (_startX2-x2), (_startY2-y2),x1,x2,y1,y2,track,zoomXDiff,zoomYDiff,ScrollDelayCount,rotateMode,rotateDone);
    
    int tmp_x1Diff =0, tmp_x2Diff = 0, tmp_y1Diff = 0, tmp_y2Diff = 0;
    
    tmp_x1Diff = x1_Diff;
    tmp_x2Diff = x2_Diff;
    tmp_y1Diff = y1_Diff;
    tmp_y2Diff = y2_Diff;
    
    //Converting negative values to Positive
    if(x1_Diff<0)
        x1_Diff = 0 - x1_Diff;
    if(y1_Diff<0)
        y1_Diff = 0 - y1_Diff;
    if(x2_Diff<0)
        x2_Diff = 0 - x2_Diff;
    if(y2_Diff<0)
        y2_Diff = 0 - y2_Diff;
    //
    
    if((x1-x2) == 0)
        rotateXCounter++;
    else if((y1-y2) == 0)
        rotateYCounter++;
    
    /*if((y1-y2)!=0 && partialRotateR == 0)
     partialRotateR++;
     else if((y1-y2) == 0 && partialRotateR>0)
     partialRotateR++;*/
    
    /*if((x1-x2)!=0 && partialRotateL == 0)
     partialRotateL++;
     else if((x1-x2) == 0 && partialRotateL>0)
     partialRotateL++;*/
    
    if(rotateXStart == 0 || rotateYStart == 0)
    {
        if((x1-x2) == 0)
            rotateXStart = rotateYStart + 1;
        else if ((y1 - y2) == 0)
            rotateYStart = rotateXStart + 1;
    }
    
    //Circular Rotate detection
    if(((rotateXStart == 2 && rotateYStart == 1 && rotateXCounter>1 && rotateYCounter>1) || (rotateXCounter>4 && partialRotateL>5)))
    {
        if(y1>y2)
            rotateCirLeft = true;
        else if(y1<y2)
            rotateCirRight = true;
        
        return;
    }
    else if(((rotateXStart == 1 && rotateYStart == 2 && rotateXCounter>1 && rotateYCounter>1) || (rotateYCounter>4 && partialRotateR>5)))
    {
        if(x1<x2)
            rotateCirRight = true;
        else if(x1>x2)
            rotateCirLeft = true;
        
        return;
    }
    //
    
    //////////Pinch Zoom
    if(((x1<_lastX1 && x2>_lastX2) ||((y1<_lastY1 && y2>_lastY2) &&
                                      (x1<_lastX1 && x2>_lastX2)) || ((y1>_lastY1&& y2<_lastY2) && zoomYDiff>300 && zoomXDiff<100))
       
       && !zoomOut && !zoomIn && !rotateDone  && !rotateMode && !rotateCirLeft && !rotateCirRight
       && ScrollDelayCount>2 && (zoomXDiff>250 || zoomYDiff>250)
       && !(touchmode == MODE_HSCROLL || touchmode == MODE_VSCROLL) && _enablePinchZoom  && !_accidentalInput)
    {
        if(x1>x2 || (y1<y2 && x1 == x2))
        {
            if(rotateXCounter == 0 && rotateYCounter == 0)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomMinus);
                zoomDone = true;
                zoomIn = true;
            }
            else
                zoomIn = true;
        }
        else if(x1<x2 || (y1>y2 && x1 == x2))
        {
            if(rotateXCounter == 0 && rotateYCounter == 0)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomPlus);
                zoomDone = true;
                zoomOut = true;
            }
            else
                zoomOut = true;
        }
        
    }
    else if(((x1>_lastX1 && x2<_lastX2) || ((y1>_lastY1&& y2<_lastY2) &&
                                            (x1>_lastX1 && x2<_lastX2)) || ((y1<_lastY1 && y2>_lastY2) && zoomYDiff>300))
            
            && !zoomIn && !zoomOut && !rotateDone && !rotateMode && !rotateCirLeft && !rotateCirRight
            && ScrollDelayCount>2 && (zoomXDiff>250 || zoomYDiff>250)
            && !(touchmode == MODE_HSCROLL || touchmode == MODE_VSCROLL) && _enablePinchZoom  && !_accidentalInput)
    {
        
        if(x1>x2 || (y1<y2 && x1 == x2))
        {
            if(rotateXCounter == 0 && rotateYCounter == 0)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomPlus);
                zoomDone = true;
                zoomOut = true;
            }
            else
                zoomOut = true;
        }
        else if(x1<x2 || (y1>y2 && x1 == x2))
        {
            if(rotateXCounter == 0 && rotateYCounter == 0)
            {
                _device->dispatchPS2Notification(kPS2C_ZoomMinus);
                zoomDone = true;
                zoomIn = true;
            }
            else
                zoomIn = true;
        }
        
    }//////////Arc Rotations
    else if((x1-x2) != 0 && (y1-y2) != 0 &&
            (((x1_Diff + y1_Diff)>(x2_Diff + y2_Diff) && (tmp_x1Diff<0 && tmp_y1Diff>0)
              && x2_Diff<200 && y2_Diff<200 && (x2_Diff<50 || y2_Diff<50)) ||
             
             (x2_Diff>(x1_Diff + y1_Diff + y2_Diff) && (tmp_x2Diff<0)
              && x1_Diff<200 && y1_Diff<200 && y2_Diff<200 && (x1_Diff<50 || y1_Diff<50 || y2_Diff<50)) ||
             
             ((x2_Diff + y2_Diff)>(y1_Diff + x1_Diff) && (tmp_x2Diff<0 && tmp_y2Diff<0)
              && x1_Diff<200 && y1_Diff<200 && (x1_Diff<50 || y1_Diff<50))) &&
            
            !zoomDone && !rotateDone && !rotateCirRight && !rotateCirRight && ScrollDelayCount>25 && (zoomXDiff>300 || zoomYDiff>300) && rotateMode && _enableRotate  && !_accidentalInput)
    {
        //Clearing the drag if started accidentally during two finger mode
        if((dragging || holdDragDelayCount>20) && rotateMode)
            
        {
            dragging = false;
            buttons = 0x0;
            dispatchRelativePointerEvent(0, 0, 0, now);
        }
        
        //
        
        
        _device->dispatchPS2Notification(kPS2C_RotateRight);
        
        rotateDone = true;
        rotateMode = true;
        return;
    }
    else if((x1-x2) != 0 && (y1-y2) != 0 &&
            (((x1_Diff + y1_Diff)>(x2_Diff + y2_Diff) && (tmp_x1Diff>0 && tmp_y1Diff<0) &&
              x2_Diff<200 && y2_Diff<200 && (x2_Diff<50 || y2_Diff<50) && (zoomXDiff>200 && zoomYDiff>200)) ||
             
             (x2_Diff>(x1_Diff + y1_Diff + y2_Diff) && tmp_x2Diff>0 &&
              x1_Diff<200 && y1_Diff<200 && y2_Diff<200 && (x1_Diff<50 || y1_Diff<50 || y2_Diff<50)) ||
             
             ((x2_Diff + y2_Diff)>(y1_Diff + x1_Diff) && (tmp_x2Diff>0 && tmp_y2Diff>0) &&
              x1_Diff<200 && y1_Diff<200 && (x1_Diff<50 || y1_Diff<50))) &&
            
            !zoomDone && !rotateDone && !rotateCirRight && !rotateCirRight && ScrollDelayCount>25  && (zoomXDiff>300 || zoomYDiff>300) && rotateMode && _enableRotate  && !_accidentalInput)
    {
        //Clearing the drag if started accidentally during two finger mode
        if((dragging || holdDragDelayCount>20) && rotateMode)
            
        {
            dragging = false;
            buttons = 0x0;
            dispatchRelativePointerEvent(0, 0, 0, now);
        }
        
        //
        
        _device->dispatchPS2Notification(kPS2C_RotateLeft);
        
        rotateDone = true;
        rotateMode = true;
        return;
    }
    //////////Two finger Scrolling
    else if(!rotateDone)
    {
        
        // hgsscrollCounter,hlsscrollCounter,vgsscrollCounter,vlsscrollCounter,slowScrollDelay
        // These variabes are used to trackdown the slow scrolling with fingers
        
        if(!_disableTouch &&
           (touchmode == MODE_MOVE || touchmode == MODE_HSCROLL ||
            touchmode == MODE_VSCROLL || touchmode == MODE_2_FING_TAP))
        {
            //Unexpected jumps fix
            if(_xdiff_2<=0 && xdiff_2>0)
                tempxd_2 = xdiff_2 - _xdiff_2;
            else if(_xdiff_2>=0 && xdiff_2<0)
                tempxd_2 = _xdiff_2 - xdiff_2;
            else
                tempxd_2 = _xdiff_2 - xdiff_2;
            
            if(_ydiff_2<=0 && ydiff_2>0)
                tempyd_2 = ydiff_2 - _ydiff_2;
            else if(_ydiff_2>=0 && ydiff_2<0)
                tempyd_2 = _ydiff_2 - ydiff_2;
            else
                tempyd_2 = _ydiff_2 - ydiff_2;
            
            //IOLog("Elan: TempXD %d TempYD %d Ldx %d Ldy %d\n",tempxd_2,tempyd_2,_xdiff_2,_ydiff_2);
            
            if((_xdiff_2<6 && _xdiff_2>-6) || (_ydiff_2<6 && _ydiff_2>-6))
                maxUnXJmp = 10;
            else if((_xdiff_2<20 && _xdiff_2>-20) || (_ydiff_2<20 && _ydiff_2>-20))
                maxUnXJmp = 20;
            
            //End fix
            
            if(track == 0  || (tempyd_2>=maxUnXJmp && _ydiff_2<50 && _ydiff_2>-50) || (_xdiff_2<50 && _xdiff_2>-50 && tempxd_2>=maxUnXJmp))
            {
                //s_xdiff = s_ydiff = 0;
                //_xdiff_2 = _ydiff_2 = 0;
                if(touchmode == MODE_HSCROLL)
                    lastmode = 0;
                else if(touchmode == MODE_VSCROLL)
                    lastmode = 1;
            }
            
            else
                maxUnXJmp = 0;
            
            int tracker = 0;
            //Horizontal Scrolling
            if(((lastmode == 0 && maxUnXJmp>0) || maxUnXJmp == 0) && ydiff_2<100 && ydiff_2>-100)
            {
                if (xdiff_2 > 0 && xdiff_2 <15 && xdiff_2>ydiff_2 && ((ydiff_2<5 && ydiff_2>-5) || touchmode == MODE_HSCROLL))
                {
                    s_xdiff = -2;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 2;
                    hgsscrollCounter++;
                    hlsscrollCounter = 0;
                    hScroll++;
                    tracker = 1;
                }
                else if (xdiff_2 >= 15 && xdiff_2 <=40 && xdiff_2>ydiff_2)
                {
                    s_xdiff = -2;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 2;
                    hScroll++;
                    tracker = 2;
                    
                }
                else if(xdiff_2 >40 && xdiff_2<=100 && xdiff_2>ydiff_2)
                {
                    s_xdiff = -3;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 3;
                    
                }
                else if(xdiff_2 >100 && xdiff_2>ydiff_2)
                {
                    s_xdiff = -4;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 4;
                    
                }
                else if(xdiff_2 >500 && xdiff_2>ydiff_2)
                {
                    s_xdiff = -5;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 5;
                }
                else if (xdiff_2 < 0 && xdiff_2 > -15 && xdiff_2<ydiff_2 && ((ydiff_2<5  && ydiff_2>-5) || touchmode == MODE_HSCROLL))
                {
                    s_xdiff = 2;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 2;
                    hlsscrollCounter++;
                    hgsscrollCounter = 0;
                    hScroll++;
                    tracker = 6;
                    
                }
                else if (xdiff_2 <= -15 && xdiff_2 >=-40 && xdiff_2<ydiff_2)
                {
                    s_xdiff = 2;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 2;
                    hScroll++;
                    tracker = 7;
                }
                else if(xdiff_2 <-40 && xdiff_2 >=-100 && xdiff_2<ydiff_2)
                {
                    s_xdiff = 3;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 8;
                    
                }
                else if(xdiff_2 <-100 && xdiff_2<ydiff_2)
                {
                    s_xdiff = 4;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 9;
                    
                }
                else if(xdiff_2 <-500 && xdiff_2<ydiff_2)
                {
                    s_xdiff = 5;
                    touchmode = MODE_HSCROLL;
                    slowScrollDelay = 1;
                    hScroll++;
                    tracker = 10;
                    
                }
                
            }
            
            if(((lastmode == 1 && maxUnXJmp>0) || maxUnXJmp == 0))
            {
                //Vertical Scrolling
                if (ydiff_2 > 0 && ydiff_2 <20 && ydiff_2>xdiff_2 && ((xdiff_2<5 && xdiff_2>-5) || touchmode == MODE_VSCROLL))
                {
                    s_ydiff = -2;
                    slowScrollDelay = 2;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 11;
                    
                    
                }
                else if (ydiff_2 >= 20 && ydiff_2 <=40 && ydiff_2>xdiff_2)
                {
                    
                    s_ydiff = -2;
                    slowScrollDelay = 2;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 12;
                    
                    
                    
                }
                else if(ydiff_2 >40 && ydiff_2 <=80 && ydiff_2>xdiff_2)
                {
                    s_ydiff = -2;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 13;
                    
                }
                else if(ydiff_2 >80 && ydiff_2 <=100 && ydiff_2>xdiff_2)
                {
                    s_ydiff = -3;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 13;
                    
                }
                
                else if(ydiff_2>100 && ydiff_2>=xdiff_2)
                {
                    s_ydiff = -4;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 14;
                    
                }
                else if (ydiff_2 < 0 && ydiff_2 >-20 && ydiff_2<xdiff_2 && ((xdiff_2<5 && xdiff_2>-5) || touchmode == MODE_VSCROLL))
                {
                    s_ydiff = 2;
                    slowScrollDelay = 2;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    vlsscrollCounter++;
                    vgsscrollCounter = 0;
                    tracker = 15;
                    
                    
                }
                else if (ydiff_2 <= -20 && ydiff_2 >=-40 && ydiff_2<xdiff_2)
                {
                    s_ydiff = 2;
                    slowScrollDelay = 2;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 15;
                    
                }
                else if(ydiff_2 <-40 && ydiff_2 >=-80 && ydiff_2<xdiff_2)
                {
                    s_ydiff = 2;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 16;
                    
                    
                }
                else if(ydiff_2 <-80 && ydiff_2 >=-100 && ydiff_2<xdiff_2)
                {
                    s_ydiff = 3;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 16;
                    
                    
                }
                else if(ydiff_2<-100 && ydiff_2<xdiff_2)
                {
                    s_ydiff = 4;
                    slowScrollDelay = 1;
                    touchmode = MODE_VSCROLL;
                    vScroll++;
                    tracker = 17;
                    
                }
            }
            
            //IOLog("Elan: Tracker %d\n ",tracker);
            
            if(!_edgevscroll)
                s_ydiff = 0;  //is Vertical Scrolling on in Trackpad.prefpane?
            
            if (!_edgehscroll)
                s_xdiff = 0; //is Horizontal Scrolling on in Trackpad.prefpane?
            
            
            if(hScroll  >= slowScrollDelay &&
               (touchmode == MODE_HSCROLL || hgsscrollCounter == 3 || hlsscrollCounter == 3))
            {
                dispatchScrollWheelEvent(0, (s_xdiff*scrollFactor), 0, now);
                hScroll = 0;
                slowScrollDelay = 0;
                hlsscrollCounter = 0;
                hgsscrollCounter = 0;
            }
            else if((vScroll >= slowScrollDelay) && touchmode == MODE_VSCROLL /*|| vgsscrollCounter == 3 || vlsscrollCounter == 3*/)
            {
                
                dispatchScrollWheelEvent(((s_ydiff*scrollFactor)), 0, 0, now);
                vScroll = 0;
                slowScrollDelay = 0;
                vgsscrollCounter = 0;
                vlsscrollCounter = 0;
            }
            
        }
    }
    
    if(ScrollDelayCount>30 && x1_Diff<35 && y1_Diff<35 && x2_Diff<35 && y2_Diff<35
       && !rotateMode  && !_accidentalInput)
    {
        touchmode = MODE_TWO_FINGERS_PRESS;
        
        _device->dispatchPS2Notification(kPS2C_TwoFingersPress);
        
        dispatchRelativePointerEvent(0, -25, 0, now);
        
        fingersPressed = true;
        
        return;
    }
    else
        touchmode = MODE_MOVE;;
    
    if(ScrollDelayCount<7  && !_accidentalInput)//Two Finger Double Tap detection
        touchmode = MODE_2_FING_TAP;

    ScrollDelayCount++;
    track=1;
    _lastX = x1;
    _lastY = y1;
    _lastX1 = x1;
    _lastY1 = y1;
    _lastX2 = x2;
    _lastY2 = y2;
    
    _xdiff_2 = xdiff_2;
    _ydiff_2 = ydiff_2;

    return;
}

/*void ApplePS2ElanTouchPad::Elantech_report_absolute_v4(ApplePS2MouseDevice *device, int packet_type)
 {
 /*switch (packet_type) {
 case PACKET_V4_STATUS:
 process_packet_status_v4(psmouse);
 break;
 
 case PACKET_V4_HEAD:
 process_packet_head_v4(psmouse);
 break;
 
 case PACKET_V4_MOTION:
 process_packet_motion_v4(psmouse);
 break;
 
 case PACKET_UNKNOWN:
 default:
 /* impossible to get here *
 break;
 }
 }
 
 static void elantech_input_sync_v4(struct psmouse *psmouse)
 {
 struct input_dev *dev = psmouse->dev;
 unsigned char *packet = psmouse->packet;
 
 input_report_key(dev, BTN_LEFT, packet[0] & 0x01);
 input_mt_report_pointer_emulation(dev, true);
 input_sync(dev);
 }
 
 static void process_packet_status_v4(struct psmouse *psmouse)
 {
 struct input_dev *dev = psmouse->dev;
 unsigned char *packet = psmouse->packet;
 unsigned fingers;
 int i;
 
 /* notify finger state change 
fingers = packet[1] & 0x1f;
for (i = 0; i < ETP_MAX_FINGERS; i++) {
    if ((fingers & (1 << i)) == 0) {
        input_mt_slot(dev, i);
        input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
    }
}

elantech_input_sync_v4(psmouse);
}

static void process_packet_head_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	int id = ((packet[3] & 0xe0) >> 5) - 1;
	int pres, traces;
    
	if (id < 0)
		return;
    
	etd->mt[id].x = ((packet[1] & 0x0f) << 8) | packet[2];
	etd->mt[id].y = etd->y_max - (((packet[4] & 0x0f) << 8) | packet[5]);
	pres = (packet[1] & 0xf0) | ((packet[4] & 0xf0) >> 4);
	traces = (packet[0] & 0xf0) >> 4;
    
	input_mt_slot(dev, id);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
    
	input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[id].x);
	input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[id].y);
	input_report_abs(dev, ABS_MT_PRESSURE, pres);
	input_report_abs(dev, ABS_MT_TOUCH_MAJOR, traces * etd->width);
	report this for backwards compatibility 
	input_report_abs(dev, ABS_TOOL_WIDTH, traces);
    
	elantech_input_sync_v4(psmouse);
}

static void process_packet_motion_v4(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct elantech_data *etd = psmouse->private;
	unsigned char *packet = psmouse->packet;
	int weight, delta_x1 = 0, delta_y1 = 0, delta_x2 = 0, delta_y2 = 0;
	int id, sid;
    
	id = ((packet[0] & 0xe0) >> 5) - 1;
	if (id < 0)
		return;
    
	sid = ((packet[3] & 0xe0) >> 5) - 1;
	weight = (packet[0] & 0x10) ? ETP_WEIGHT_VALUE : 1;
	
	 * Motion packets give us the delta of x, y values of specific fingers,
	 * but in two's complement. Let the compiler do the conversion for us.
	 * Also _enlarge_ the numbers to int, in case of overflow.
	 
	delta_x1 = (signed char)packet[1];
	delta_y1 = (signed char)packet[2];
	delta_x2 = (signed char)packet[4];
	delta_y2 = (signed char)packet[5];
    
	etd->mt[id].x += delta_x1 * weight;
	etd->mt[id].y -= delta_y1 * weight;
	input_mt_slot(dev, id);
	input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[id].x);
	input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[id].y);
    
	if (sid >= 0) {
		etd->mt[sid].x += delta_x2 * weight;
		etd->mt[sid].y -= delta_y2 * weight;
		input_mt_slot(dev, sid);
		input_report_abs(dev, ABS_MT_POSITION_X, etd->mt[sid].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, etd->mt[sid].y);
	}
    
	elantech_input_sync_v4(psmouse);
}

 */



//
//From Apple Sources for Trackpad
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::setCommandByte( UInt8 setBits, UInt8 clearBits )
{
    //
    // Sets the bits setBits and clears the bits clearBits "atomically" in the
    // controller's Command Byte.   Since the controller does not provide such
    // a read-modify-write primitive, we resort to a test-and-set try loop.
    //
    // Do NOT issue this request from the interrupt/completion context.
    //
    
    UInt8        commandByte;
    UInt8        commandByteNew;
    PS2Request * request = _device->allocateRequest();
    
    if ( !request ) return;
    
    do
    {
        // (read command byte)
        request->commands[0].command = kPS2C_WriteCommandPort;
        request->commands[0].inOrOut = kCP_GetCommandByte;
        request->commands[1].command = kPS2C_ReadDataPort;
        request->commands[1].inOrOut = 0;
        request->commandsCount = 2;
        _device->submitRequestAndBlock(request);
        
        //
        // Modify the command byte as requested by caller.
        //
        
        commandByte    = request->commands[1].inOrOut;
        commandByteNew = (commandByte | setBits) & (~clearBits);
        
        // ("test-and-set" command byte)
        request->commands[0].command = kPS2C_WriteCommandPort;
        request->commands[0].inOrOut = kCP_GetCommandByte;
        request->commands[1].command = kPS2C_ReadDataPortAndCompare;
        request->commands[1].inOrOut = commandByte;
        request->commands[2].command = kPS2C_WriteCommandPort;
        request->commands[2].inOrOut = kCP_SetCommandByte;
        request->commands[3].command = kPS2C_WriteDataPort;
        request->commands[3].inOrOut = commandByteNew;
        request->commandsCount = 4;
        _device->submitRequestAndBlock(request);
        
        //
        // Repeat this loop if last command failed, that is, if the
        // old command byte was modified since we first read it.
        //
        
    } while (request->commandsCount != 4);
    
    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::receiveKeyboardNotifications(UInt32 data)
{
    AbsoluteTime now;
    
#if APPLESDK
    clock_get_uptime(&now);
#else
    clock_get_uptime((uint64_t*)&now);
#endif
    if(_enableTypingMode)
    switch (data) {
        case kPS2C_DisableTouchpad:
            //IOLog("Received Palm/Accidental Input mode\n");
            if(accidentalInputKeys>2)
            _accidentalInput = true;
            lastKeyPressTime = now;
            break;
        case kPS2C_EnableTouchpad:
            lastKeyPressTime = now;
            accidentalInputKeys++;

            break;
        default:
            break;
    }
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::setDevicePowerState( UInt32 whatToDo )
{
    switch ( whatToDo )
    {
        case kPS2C_DisableDevice:
            
            //
            // Disable touchpad (synchronous).
            //
            
            Elantech_Touchpad_enable( false );
            break;
            
        case kPS2C_EnableDevice:
		case 2:
            
            //
            // Must not issue any commands before the device has
            // completed its power-on self-test and calibration.
            //
            
            IOSleep(1500);
            
            /*
             * Put the touchpad back into absolute mode when reconnecting
             */
            
            
            //Detecting the Presence of Elan Touchpad
            //
            IOLog("Elan: Detecting the touchpad\n");
            Elantech_detect(_device);
            //
            
            //Placing the touchpad into absolute mode
            
            
            if (Elantech_set_absolute_mode(_device)) {
                IOLog("Elan: Failed to put touchpad into absolute mode.\n");
                return;
            }
            else
                IOLog("Elan: Successfully placed touchpad into Absolute mode\n");
            
            //Set SampleRate and Res
            setSampleRateAndResolution();
            
            getMouseInformation();
            //
            // Enable the mouse clock (should already be so) and the
            // mouse IRQ line.
            //
            
            setCommandByte( kCB_EnableMouseIRQ, kCB_DisableMouseClock );
            
            //
            // Clear packet buffer pointer to avoid issues caused by
            // stale packet fragments.
            //
            
            _packetByteCount = 0;
            
            //
            // Finally, we enable the trackpad itself, so that it may
            // start reporting asynchronous events.
            //
            IOLog("Elan: Enabling the touchpad\n");
            Elantech_Touchpad_enable( true );
            
            //Releasing Command Key so we can change to normal mode
            //before sleep the command is pressed down preventing the restart by mac when pressing sleep
            //Still we have to press once Commnad key once before it works with Command Operations
            _device->dispatchPS2Notification(kPS2C_ReleaseKey);
            break;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ElanTouchPad::setTouchPadModeByte( UInt8 modeByteValue,
                                               bool  enableStreamMode )
{
    PS2Request * request = _device->allocateRequest();
    bool         success;
    
    //IOLog("Elan:  Placed the touchMode %d\n",_touchPadModeByte);
    if ( !request ) return false;
    
    // Disable stream mode before the command sequence.
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetDefaultsAndDisable;
    
    // 4 set resolution commands, each encode 2 data bits.
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = kDP_SetMouseResolution;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = (modeByteValue >> 6) & 0x3;
    
    request->commands[3].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut  = kDP_SetMouseResolution;
    request->commands[4].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut  = (modeByteValue >> 4) & 0x3;
    
    request->commands[5].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[5].inOrOut  = kDP_SetMouseResolution;
    request->commands[6].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[6].inOrOut  = (modeByteValue >> 2) & 0x3;
    
    request->commands[7].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[7].inOrOut  = kDP_SetMouseResolution;
    request->commands[8].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[8].inOrOut  = (modeByteValue >> 0) & 0x3;
    
    // Set sample rate 20 to set mode byte 2. Older pads have 4 mode
    // bytes (0,1,2,3), but only mode byte 2 remain in modern pads.
    request->commands[9].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[9].inOrOut  = kDP_SetMouseSampleRate;
    request->commands[10].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[10].inOrOut = 20;
    
    request->commands[11].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[11].inOrOut  = enableStreamMode ?
    kDP_Enable :
    kDP_SetMouseScaling1To1; /* Nop */
    
    request->commandsCount = 12;
    _device->submitRequestAndBlock(request);
    
    success = (request->commandsCount == 12);
    
    _device->freeRequest(request);
    
    if(success)
        IOLog("Elan: Sucessfully placed the touchMode %d\n",_touchPadModeByte);
    else
        IOLog("Elan: Failed to set the touchMode %d\n",_touchPadModeByte);
    
    return success;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::getMouseInformation()
{
	UInt8 Byte1, Byte2, Byte3;
    PS2Request * request = _device->allocateRequest();
    
    if ( !request ) return;
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_GetMouseInformation;
    request->commands[1].command = kPS2C_ReadDataPort;
    request->commands[1].inOrOut = 0;
    request->commands[2].command = kPS2C_ReadDataPort;
    request->commands[2].inOrOut = 0;
    request->commands[3].command = kPS2C_ReadDataPort;
    request->commands[3].inOrOut = 0;
    request->commandsCount = 4;
    _device->submitRequestAndBlock(request);
	Byte1 = request->commands[1].inOrOut;
	Byte2 = request->commands[2].inOrOut;
	Byte3 = request->commands[3].inOrOut;
    IOLog("Elan: MouseInformation { 0x%02x, 0x%02x, 0x%02x }\n", Byte1, Byte2, Byte3);
	_device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::getStatus(ELANStatus_type *status)
{
    PS2Request * request = _device->allocateRequest();
    
    if ( !request ) return;
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut = kDP_GetMouseInformation;
    request->commands[4].command = kPS2C_ReadDataPort;
    request->commands[4].inOrOut = 0;
    request->commands[5].command = kPS2C_ReadDataPort;
    request->commands[5].inOrOut = 0;
    request->commands[6].command = kPS2C_ReadDataPort;
    request->commands[6].inOrOut = 0;
    request->commandsCount = 7;
    _device->submitRequestAndBlock(request);
    status->Byte1 = request->commands[4].inOrOut;
    status->Byte2 = request->commands[5].inOrOut;
    status->Byte3 = request->commands[6].inOrOut;
    IOLog("Elan: MouseStatus { 0x%02x, 0x%02x, 0x%02x }\n", status->Byte1, status->Byte2, status->Byte3);
    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//This is not working for our Elan

void ApplePS2ElanTouchPad::setTapEnable( bool enable )
{
    //
    // Instructs the trackpad to honor or ignore tapping
    //
	ELANStatus_type Status;
	bool success;
	
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;
    
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[3].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[4].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut  = kDP_GetMouseInformation;
    request->commands[5].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[5].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[6].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[6].inOrOut  = kDP_SetDefaultsAndDisable;
    
	getStatus(&Status);
	if (Status.Byte1 & 0x04)
    {
        //DEBUG_LOG("Tapping can only be toggled.\n");
		enable = false;
	}
    
    if (enable)
	{
		request->commands[7].command = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[7].inOrOut = kDP_SetMouseSampleRate;
		request->commands[8].command = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[8].inOrOut = 0x0A;
	}
	else
	{
		request->commands[7].command = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[7].inOrOut = kDP_SetMouseResolution;
		request->commands[8].command = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[8].inOrOut = 0x00;
	}
    
    request->commands[9].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[9].inOrOut = kDP_SetMouseScaling1To1;
    request->commands[10].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[10].inOrOut = kDP_SetMouseScaling1To1;
    request->commands[11].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[11].inOrOut = kDP_SetMouseScaling1To1;
    request->commands[12].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[12].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[13].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[13].inOrOut = kDP_Enable;
    request->commandsCount = 14;
	_device->submitRequestAndBlock(request);
    
	getStatus(&Status);
    
    success = (request->commandsCount == 14);
	if (success)
	{
		setSampleRateAndResolution();
        IOLog("Elan: Tapping Enabled\n");
	}
    else
        IOLog("Elan: Tapping Failed\n");
    
    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ElanTouchPad::setSampleRateAndResolution( void )
{
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_SetDefaultsAndDisable;           // 0xF5, Disable data reporting
    request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut = kDP_SetMouseSampleRate;              // 0xF3
    request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut = 0x64;                                // 100 dpi
    request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut = kDP_SetMouseResolution;              // 0xE8
    request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut = 0x03;                                // 0x03 = 8 counts/mm
    request->commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[5].inOrOut = kDP_SetMouseScaling1To1;             // 0xE6
    request->commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[6].inOrOut = kDP_Enable;                          // 0xF4, Enable Data Reporting
    request->commandsCount = 7;
    _device->submitRequestAndBlock(request);
    _device->freeRequest(request);
    
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -