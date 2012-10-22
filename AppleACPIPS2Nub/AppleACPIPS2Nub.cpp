/*! @file       ACPIPS2Nub.h
 @abstract   AppleACPIPS2Nub class implementation
 @discussion
 Implements the ACPI PS/2 nub for ApplePS2Controller.kext.
 Reverse-engineered from the Darwin 8 binary ACPI kext.
 Copyright 2007 David Elliott
 */

#include "AppleACPIPS2Nub.h"
#include <IOKit/acpi/IOACPITypes.h>

#if 0
#define DEBUG_LOG(args...)  DEBUG_LOG(args)
#else
#define DEBUG_LOG(args...)
#endif

static IOPMPowerState myTwoStates[2] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

OSDefineMetaClassAndStructors(AppleACPIPS2Nub, IOPlatformDevice);

// FIXME: We could simply ask for the PE rather than importing the global
// from AppleACPIPlatformExpert.kext
//extern IOPlatformExpert *gAppleACPIPlatformExpert;


bool AppleACPIPS2Nub::start(IOService *provider)
{
    if(!IOPlatformDevice::start(provider))
        return false;
    
    DEBUG_LOG("Starting AppleACPIPS2Nub\n");
    
    /* Initialize our interrupt controller/specifier i-vars */
    m_interruptControllers = OSArray::withCapacity(2);
    m_interruptSpecifiers = OSArray::withCapacity(2);
    if(m_interruptControllers == NULL || m_interruptSpecifiers == NULL)
        return false;
    
    /* Merge in the keyboard (primary) provider interrupt properties */
    mergeInterruptProperties(provider, LEGACY_KEYBOARD_IRQ);
    
    /* Initialize and register our power management properties */
    PMinit();
    //////////Original
    /*static int const PowerStateCount = 3;
    static IOPMPowerState const PowerStateArray[PowerStateCount]  =
    {   {kIOPMPowerStateVersion1}
        ,   {kIOPMPowerStateVersion1, kIOPMDeviceUsable, kIOPMDoze, kIOPMDoze}
        ,   {kIOPMPowerStateVersion1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn}
    };
    registerPowerDriver(this, const_cast<IOPMPowerState*>(PowerStateArray), PowerStateCount);*/
    /////////Alternative from mydellmini
    registerPowerDriver(this, myTwoStates, 2);
    provider->joinPMtree(this);
    
    /* Find the mouse provider */
    m_mouseProvider = findMouseDevice();
    if(m_mouseProvider != NULL)
    {
        DEBUG_LOG("Found mouse PNP device\n");
        if(attach(m_mouseProvider))
        {
            mergeInterruptProperties(m_mouseProvider, LEGACY_MOUSE_IRQ);
            if(m_mouseProvider->inPlane(gIOPowerPlane))
            {
                m_mouseProvider->joinPMtree(this);
            }
            /* Set our interrupt properties in the IOReigstry */
            if(m_interruptControllers->getCount() != 0 && m_interruptSpecifiers->getCount() != 0)
            {
                setProperty(gIOInterruptControllersKey, m_interruptControllers);
                setProperty(gIOInterruptSpecifiersKey, m_interruptSpecifiers);
                DEBUG_LOG("AppleACPIPS2Nub startup complete\n");
            }
        }
    }
    /* Release the arrays we allocated.  Our properties dictionary has them retained */
    m_interruptControllers->release();
    m_interruptControllers = NULL;
    m_interruptSpecifiers->release();
    m_interruptSpecifiers = NULL;
    
    /* Make ourselves the ps2controller nub and register so ApplePS2Controller can find us. */
    setName("ps2controller");
    registerService();
    return true;
}


IOService* AppleACPIPS2Nub::findMouseDevice()
{
    OSObject *prop = getProperty("MouseNameMatch");
    /* Search from the root of the ACPI plane for the mouse PNP nub */
    IORegistryIterator *i = IORegistryIterator::iterateOver(gIOACPIPlane, kIORegistryIterateRecursively);
    IORegistryEntry *entry;
    if(i != NULL)
    {
        while(entry = i->getNextObject())
        {
            if(entry->compareNames(prop))
                break;
        }
        i->release();
    }
    else
        entry = NULL;
    return OSDynamicCast(IOService, entry);
}


bool AppleACPIPS2Nub::finalize(IOOptionBits options)
{
    PMstop();
    return IOPlatformDevice::finalize(options);
}


void AppleACPIPS2Nub::free()
{
    IOPlatformDevice::free();
}


void AppleACPIPS2Nub::mergeInterruptProperties(IOService *pnpProvider, long)
{
    /* Make sure we're called from within start() where these i-vars are valid */
    if(m_interruptControllers == NULL || m_interruptSpecifiers == NULL)
        return;
    
    /*  Get the interrupt controllers/specifiers arrays from the provider, and make sure they
     *  exist and contain at least one entry.  We assume they contain exactly one entry.
     */
    OSArray *controllers = OSDynamicCast(OSArray,pnpProvider->getProperty(gIOInterruptControllersKey));
    OSArray *specifiers = OSDynamicCast(OSArray,pnpProvider->getProperty(gIOInterruptSpecifiersKey));
    if(controllers == NULL || specifiers == NULL)
        return;
    if(controllers->getCount() == 0 || specifiers->getCount() == 0)
        return;
    
    /* Append the first object of each array into our own respective array */
    m_interruptControllers->setObject(controllers->getObject(0));
    m_interruptSpecifiers->setObject(specifiers->getObject(0));
}


IOReturn AppleACPIPS2Nub::registerInterrupt(int source, OSObject *target, IOInterruptAction handler, void *refCon)
{
    if(source == LEGACY_KEYBOARD_IRQ)
        return IOPlatformDevice::registerInterrupt(0, target, handler, refCon);
    else if(source == LEGACY_MOUSE_IRQ)
        return IOPlatformDevice::registerInterrupt(1, target, handler, refCon);
    else
        return kIOReturnBadArgument;
}

IOReturn AppleACPIPS2Nub::unregisterInterrupt(int source)
{
    if(source == LEGACY_KEYBOARD_IRQ)
        return IOPlatformDevice::unregisterInterrupt(0);
    else if(source == LEGACY_MOUSE_IRQ)
        return IOPlatformDevice::unregisterInterrupt(1);
    else
        return kIOReturnBadArgument;
}

IOReturn AppleACPIPS2Nub::getInterruptType(int source, int *interruptType)
{
    if(source == LEGACY_KEYBOARD_IRQ)
        return IOPlatformDevice::getInterruptType(0, interruptType);
    else if(source == LEGACY_MOUSE_IRQ)
        return IOPlatformDevice::getInterruptType(1, interruptType);
    else
        return kIOReturnBadArgument;
}


IOReturn AppleACPIPS2Nub::enableInterrupt(int source)
{
    if(source == LEGACY_KEYBOARD_IRQ)
        return IOPlatformDevice::enableInterrupt(0);
    else if(source == LEGACY_MOUSE_IRQ)
        return IOPlatformDevice::enableInterrupt(1);
    else
        return kIOReturnBadArgument;
}

IOReturn AppleACPIPS2Nub::disableInterrupt(int source)
{
    if(source == LEGACY_KEYBOARD_IRQ)
        return IOPlatformDevice::disableInterrupt(0);
    else if(source == LEGACY_MOUSE_IRQ)
        return IOPlatformDevice::disableInterrupt(1);
    else
        return kIOReturnBadArgument;
}


bool AppleACPIPS2Nub::compareName( OSString * name, OSString ** matched ) const
{
    //return gAppleACPIPlatformExpert->compareNubName(this, name, matched);
    return( this->IORegistryEntry::compareName( name, matched ));

}


IOReturn AppleACPIPS2Nub::getResources( void )
{
    //return gAppleACPIPlatformExpert->getNubResources(this);
    return( kIOReturnSuccess );

}
