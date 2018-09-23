#ifndef _CAM_NOTIFY_H_
#define _CAM_NOTIFY_H_

/**
    callback path , callback at a user-inidicated timing.
*/
class P1_TUNING_NOTIFY
{
    public:
    virtual ~P1_TUNING_NOTIFY(){}
    virtual const char* TuningName(void) = 0;
    virtual void p1TuningNotify(MVOID *pInput,MVOID *pOutput)= 0;

    MVOID*  m_pClassObj;
};


#endif

