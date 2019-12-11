#ifndef PDCCHECKER
#define PDCCHECKER

#include "interfaces.h"
#include "objects.h"
#include "project.h"
#include "scheduledata.h"

namespace A653 {

struct myTreeNode
{
    ObjectId mPart; // partition id
    CoreId mCore; // core id for partition
    myTreeNode();
    myTreeNode(ObjectId part, CoreId core = CoreId());
    myTreeNode(const myTreeNode &t);
    bool operator ==(const myTreeNode &t) const;
    bool operator !=(const myTreeNode &t) const;
};

class PDCChecker
{
public:
    QMap<ObjectId,CoreId> partsConstr;
    static bool checkPDC (QMap <ObjectId, double> &messageDur, const QList<myTreeNode> &myList, ScheduleData *data, bool onlyCheck,
                           QMultiMap<ObjectId, QSet<ObjectId>> &partsSDI /*īķņōîé â íā÷āëå- åãî ōîëüęî âîįâđāųāåė*/, QMap<ObjectId,CoreId> &fixedParts,
                          QMap<ObjectId, double> &mesConstr,const QMap<ObjectId, double> &minChainMes,const QMap<ObjectId, double> &mesMaxDur);

    static QMap<ObjectId, CoreId> initTaskCore (const QList<myTreeNode> &myList, const  QList<Task*> tasks);

};

}

#endif // PDCCHECKER
