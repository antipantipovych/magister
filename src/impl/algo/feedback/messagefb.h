#ifndef MESSAGEFB
#define MESSAGEFB

#include "scheduledata.h"

namespace A653 {


class MessageFB
{
public:
    static void countMesFB (ScheduleData* data, const QList<Message*> &failedMes, QMap<ObjectId,double> &moduleThr,
                            const QMap<ObjectId, CoreId> &taskCore, const QList<ObjectId> &failedMod);
};
}

#endif // MESSAGEFB

