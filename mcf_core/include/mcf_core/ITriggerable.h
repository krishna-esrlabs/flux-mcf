/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_ITRIGGERABLE_H
#define MCF_ITRIGGERABLE_H

namespace mcf {

/**
 *  An ITriggerable object is an object that can be registered to be triggered by a TriggerSource
 */
class ITriggerable {
public:

    /**
     * Method to be called when trigger event occurs
     */
    virtual void trigger() = 0;

protected:
    ITriggerable() = default;
};

}

#endif // MCF_ITRIGGERABLE_H
