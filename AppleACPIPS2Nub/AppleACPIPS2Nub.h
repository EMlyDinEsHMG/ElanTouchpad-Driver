/*! @file       ACPIPS2Nub.h
 @abstract   AppleACPIPS2Nub class definition
 @discussion
 Implements the ACPI PS/2 nub for ApplePS2Controller.kext.
 Reverse-engineered from the Darwin 8 binary ACPI kext.
 Copyright 2007 David Elliott
 */

#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>

/*! @class      AppleACPIPS2Nub
 @abstract   Provides a nub that ApplePS2Controller can attach to
 @discussion
 The ApplePS2Controller driver is written to the nub provided by the
 AppleI386PlatformExpert class which exposes the controller as it is found
 on a legacy x86 machine.
 
 To make that kext work on an ACPI machine, a nub providing the same
 service must exist.  Previous releases of the official ACPI PE included
 this class.  Newer release do not.  This implementation is intended
 to be fully ABI compatible with the one Apple used to provide.
 */
class AppleACPIPS2Nub: public IOPlatformDevice
{
    OSDeclareDefaultStructors(AppleACPIPS2Nub);
private:
    /*! @field      m_mouseProvider
     @abstract   Our second provider which provides the mouse nub
     @discussion
     We attach to the keyboard nub but we need to be attached to both the
     keyboard and mouse nub in order to make ourselves a proper nub for
     the ApplePS2Controller.kext driver.
     */
    IOService *m_mouseProvider;
    
    void *m__pad; // This was present but never used in the Apple code
    
    /*! @field      m_interruptControllers
     @abstract   Our array of interrupt controllers
     */
    OSArray *m_interruptControllers;
    
    /*! @field      m_interruptSpecifiers
     @abstract   Our array of interrupt specifiers
     */
    OSArray *m_interruptSpecifiers;
    
    enum LegacyInterrupts
    {   LEGACY_KEYBOARD_IRQ = 1
        ,   LEGACY_MOUSE_IRQ = 12
    };
    
    static inline bool convertInterruptNumber(int &source)
    {
        if(source == LEGACY_KEYBOARD_IRQ)
            source = 0;
        else if(source == LEGACY_MOUSE_IRQ)
            source = 1;
        else
            return false;
        return true;
    }
    
public:
    virtual bool start(IOService *provider);
    
    /*! @method     findMouseDevice
     @abstract   Locates the mouse nub in the IORegistry
     */
    virtual IOService* findMouseDevice();
    
    virtual bool finalize( IOOptionBits options );
    virtual void free(void);
    
    /*! @method     mergeInterruptProperties
     @abstract   Merges the interrupt specifiers and controllers from our two providers
     @param  pnpProvider     The provider nub
     @discussion
     This is called once for each of our providers.  The interrupt controller and interrupt
     specifier objects from the provider's arrays are appended to our arrays.
     */
    virtual void mergeInterruptProperties(IOService *pnpProvider, long source);
    
    /*! @method     registerInterrupt
     @abstract   Overriden to translate the legacy interrupt numbers to ours
     @discussion
     The legacy interrupts are 1 for the keyboard and 12 (0xc) for the mouse.
     However, the base class code works off of the controller and specifier
     objects in our IOInterruptControllers and IOInterruptSpecifiers keys.
     Therefore, we must translate the keyboard interrupt (1) to the index
     into our array (0) and the mouse interrupt (12) to the index into
     our array (1).
     
     This has to be done for every *Interrupt* method
     */
    virtual IOReturn registerInterrupt(int source, OSObject *target,
                                       IOInterruptAction handler,
                                       void *refCon = 0);
    virtual IOReturn unregisterInterrupt(int source);
    virtual IOReturn getInterruptType(int source, int *interruptType);
    virtual IOReturn enableInterrupt(int source);
    virtual IOReturn disableInterrupt(int source);
    
    /*! @method     compareName
     @abstract   Overridden to call the IOPlatformExpert compareNubName method
     @discussion
     I have no idea why this is done, but the Apple code did it, so this
     code does too.
     */
    virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;
    
    /*! @method     getResources
     @abstract   Overridden to call the IOPlatformExpert getNubResources method
     @discussion
     I have no idea why this is done, but the Apple code did it, so this
     code does too.
     */
    virtual IOReturn getResources( void );
};
