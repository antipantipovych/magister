#ifndef XMLPROCESSOR
#define XMLPROCESSOR

#include "scheduledata.h"
#include <string>

namespace A653 {

class XMLProcessor{
public:
    static void create_afdx_xml (QString fileName, ScheduleData * data, const QMap<ObjectId, CoreId> &partCore, const QMap<ObjectId, double> &mesMaxDur);
    static void get_vl_response (std::string fileName, ScheduleData * data, QMap <ObjectId, double> &messageDur,  QList<Message*> &failedMes,
                                 const QMap<ObjectId, CoreId> &taskCore, QList<ObjectId> &failedMod);
    static void create_afdx_xml_triangle (QString fileName, ScheduleData * data, const QMap<ObjectId, CoreId> &partCore,
                                          const QMap<ObjectId, double> &mesMaxDur);
};

}
#endif // XMLPROCESSOR

