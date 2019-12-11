#ifndef A653_MYBINDALGO_H
#define A653_MYBINDALGO_H

#include "interfaces.h"
#include "objects.h"
#include "project.h"
#include "pdcchecker.h"

namespace A653 {

static constexpr double MaxThroughput = 10000000; // ёфхырЄ№ ьрёёштюь фы  ърцфюую ьюфєы 

//struct myTreeNode
//{
//	ObjectId mPart; // partition id
//	CoreId mCore; // core id for partition
//	myTreeNode();
//	myTreeNode(ObjectId part, CoreId core = CoreId());
//	myTreeNode(const myTreeNode &t);
//	bool operator ==(const myTreeNode &t) const;
//	bool operator !=(const myTreeNode &t) const;
//};

class BindAlgoBranchNB : public BindingAlgo
{
public:
    virtual bool makeBinding(Schedule* schedule);

    BindAlgoBranchNB ();
    BindAlgoBranchNB (QMultiMap<ObjectId, CoreId> ec, QMultiMap<ObjectId, QSet<ObjectId>> notTogether, QMap<ObjectId, CoreId> fixed, Schedule * sched);
    BindAlgoBranchNB (QMultiMap<ObjectId, CoreId> ec, QMultiMap<ObjectId, QSet<ObjectId>> notTogether, QMap<ObjectId, CoreId> fixed, QMap<ObjectId, double> thr);
    QList<myTreeNode> mOpt; // optimal solution
    bool fail = false;

private:
	Schedule* mSchedule;

	QMap<ObjectId, ObjectIdList> mAvailModules;	// modules avaliable for partitions
	ObjectIdList mPartsToBind;			// partitions to bind
	ObjectIdList mBound;				// already bound parts

	double mOptLoad;
    QMultiMap<ObjectId, CoreId> extraConstr;
    QMap<ObjectId, CoreId> fixedParts;
    QMap<ObjectId, double> moduleThrConstr;
    QMultiMap<ObjectId, QSet<ObjectId>> notTogether;
};

} // namespace A653

#endif // MYBINDALGO_H
