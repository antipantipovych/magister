#include "bindalgobranchandbound.h"

#include <iostream>
#include <string>
#include "math.h"

#include <QDebug>
#include <QTime>
#include <QVector>
#include <QFile>

#include "scheduledata.h"
#include "projectdata.h"
#include "pdcchecker.h"

#define EPS 0.01
#define DELTA 0.025
//#define DEBUG


namespace A653 {
double MaxPosLoad=0;
double ** partLoad;
double ** partPartChannelLoad;
double Period = 1000;
double SchedInt = 10000; //����� ����������
double CS_END = 1000; //������������ CS - ����� ������
double MIN_MES_DUR = 0.002;//���� ����� ������������ �������� ��������� �� ���������� ������?
QFile newlog("new_bound_log.txt");
QFile oldlog("allgr_b_log.txt");
//QFile oldlog("b-40_log.txt");
//QFile oldlog("gruops_log.txt");
QTextStream oldBoundStream(&oldlog);
QTextStream newBoundStream(&newlog);
struct Group{
    int num;
    //bool flag;
    QList<CoreId> groupList;
    Group(){
                //flag=false;
    }
};

struct PsevdoGroup{
    int num;
    QList<CoreId> groupList;
    QSet<double> volums;
    double groupVol;
    double midVol=-1.0;
    PsevdoGroup(){
    }
};

struct CoreUpperBound{
    CoreId coreId;
    double upperBound;
    CoreUpperBound(){}
    CoreUpperBound(CoreId c, double d){
        coreId=c;
        upperBound=d;
    }

    bool operator < (const CoreUpperBound &other) const{
        return (this->upperBound)>(other.upperBound);
    }
};

struct CoreLowerBound{
    CoreId coreId;
    double upperBound;
    double lowerBound;
    CoreLowerBound(){}
    CoreLowerBound(CoreId c, double d, double l){
        coreId=c;
        upperBound=d;
        lowerBound=l;
    }

    bool operator < (const CoreLowerBound &other) const{
        return (this->lowerBound)>(other.lowerBound);
    }
};

struct Cores1{
    CoreId coreId;
    double load;
    Cores1(){}
    Cores1(CoreId c, double d){
        coreId=c;
        load=d;
    }

    bool operator < (const Cores1 &other) const{
        return (this->load)>(other.load);//<- �� ����������� ����������� �����
    }
};

struct Cores2{
    CoreId coreId;
    double connection;
    Cores2(){}
    Cores2(CoreId c, double d){
        coreId=c;
        connection=d;
    }

    bool operator < (const Cores2 &other) const{
        return (this->connection)>(other.connection);//<- �� ����������� ������
    }
};

void initPartLoad(QList<ObjectId> parts, QList<CoreId> cores, ScheduleData *data){
    partLoad=new double*[cores.size()];
    for(int i=0; i<cores.size(); i++){
        partLoad[i]=new double[parts.size()];
    }

    foreach(CoreId c, cores){
        foreach(ObjectId p, parts){
            partLoad[c.coreNum][p.getId()]=data->partLoad(p,c);
            //partLoad[c.coreNum][p.getId()]=partLoad[c.coreNum][p.getId()];
        }
    }
}

void initPartPartChannelLoad(QList<ObjectId> parts, ScheduleData *data){
    partPartChannelLoad=new double*[parts.size()];
    for(int i=0; i<parts.size(); i++){
        partPartChannelLoad[i]=new double[parts.size()];
    }

    foreach(ObjectId s, parts){
        foreach(ObjectId r, parts){
            partPartChannelLoad[s.getId()][r.getId()]=data->partPartChannelLoad(s,r);
        }
    }
}



double addTime(const QList<myTreeNode> &myList, const myTreeNode &node, ScheduleData *data)
{
	double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore.moduleId == node.mCore.moduleId) {
            continue;
        } else {
            //tempLoad += data->partPartChannelLoad(t.mPart, node.mPart);
            tempLoad+=partPartChannelLoad[(*iter).mPart.getId()][node.mPart.getId()];
        }
    }
//	foreach (myTreeNode t, myList) {
//		if (t.mCore.moduleId == node.mCore.moduleId) {
//			continue;
//		} else {
//            //tempLoad += data->partPartChannelLoad(t.mPart, node.mPart);
//            tempLoad+=partPartChannelLoad[t.mPart.getId()][node.mPart.getId()];
//		}
//	}
	return tempLoad;
}

double addLinks(const QList<myTreeNode> & myList, const myTreeNode& node, ScheduleData *data)
{
    double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore.moduleId != node.mCore.moduleId) {
            continue;
        } else {
           // tempLoad += data->partPartChannelLoad(t.mPart, node.mPart);
            tempLoad+=partPartChannelLoad[(*iter).mPart.getId()][node.mPart.getId()];
        }
    }
//    foreach (myTreeNode t, myList) {
//        if (t.mCore.moduleId != node.mCore.moduleId) {
//            continue;
//        } else {
//           // tempLoad += data->partPartChannelLoad(t.mPart, node.mPart);
//            tempLoad+=partPartChannelLoad[t.mPart.getId()][node.mPart.getId()];
//        }
//    }
    return tempLoad;
}

double connectionWithModuleByModId(const QList<myTreeNode> &myList, const ObjectId &mod, const ObjectId part,ScheduleData *data)
{
    double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore.moduleId == mod) {
            //tempLoad += data->partLoad(t.mPart, core);
            tempLoad+=partPartChannelLoad[part.getId()][(*iter).mPart.getId()];
        } else {
            continue;
        }
    }
    return tempLoad;
}

//���������, ��� ��������� - �����? ������������? ������� ��������?

bool countEndThroughtput(const QList<myTreeNode> & myList, const CoreId &c, const ObjectId part, ScheduleData *data,
                         const QMap<ObjectId, double> &moduleThrConstr){

    QSet<ObjectId> modules;
    modules.insert(c.moduleId);
    QList<myTreeNode>::const_iterator  iterBr;
    for(iterBr = myList.begin(); iterBr!=myList.end(); iterBr++){
        modules.insert((*iterBr).mCore.moduleId);
    }
    QSet<ObjectId>::const_iterator  iterM;
    for(iterM = modules.begin(); iterM != modules.end(); iterM++){
        double sum = 0;
        if (c.moduleId != (*iterM)){
            sum += connectionWithModuleByModId(myList, (*iterM), part, data);
        }
        for (iterBr = myList.begin(); iterBr!=myList.end(); iterBr++){
           if ((*iterM) != (*iterBr).mCore.moduleId){
               sum += connectionWithModuleByModId(myList, (*iterM), (*iterBr).mPart, data);
               if (c.moduleId == (*iterM)){
                   sum+= partPartChannelLoad[part.getId()][(*iterBr).mPart.getId()];
               }
           }
        }
        if (sum > moduleThrConstr.find((*iterM)).value()) return false;
    }

    return true;
}

double countTaskDur(const ObjectId &taskId, const QMap <ObjectId, CoreId> taskCore, ScheduleData *data) {
    Task* task;
    for (int i = 0; i < (data->tasks().count()); i++){
        if (data->tasks().at(i)->id() == taskId){
            task = data->tasks().at(i);
            break;
        }
    }
    if (!task){
        return -1;
    }

    return (double)task->duration(taskCore.find(taskId).value().processorTypeId());
}

void countLeftSDI(const ObjectId &prevTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                  QMap <ObjectId, QList<double>> &leftSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data){
    double prevDur = countTaskDur(prevTask, taskCore, data);
    for (int i = 0; i < messages.size(); i++){
        if (messages.at(i)->senderId()== prevTask){

            leftSDI.find(messages.at(i)->receiverId()).value().first()=(std::max(leftSDI.find(messages.at(i)->receiverId()).value().first(),
                                                                leftSDI.find(prevTask).value().first()+prevDur+messageDur.find(messages.at(i)->id()).value()));
            countLeftSDI(messages.at(i)->receiverId(), messages, messageDur, leftSDI, taskCore, T, data);
        }
    }
    for (int i = 1; i < (int)SchedInt/T; i++){
        leftSDI.find(prevTask).value().append(leftSDI.find(prevTask).value().first()+i*T);
    }
}

void countRightSDI(const ObjectId &nextTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                  QMap <ObjectId, QList<double>> &rightSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data){
    double nextDur = countTaskDur(nextTask, taskCore, data);
    for (int i = 0; i < messages.size(); i++){
        if (messages.at(i)->receiverId()== nextTask){
            rightSDI.find(messages.at(i)->senderId()).value().first()=(std::min(rightSDI.find(messages.at(i)->senderId()).value().first(),
                                                                rightSDI.find(nextTask).value().first()- nextDur - messageDur.find(messages.at(i)->id()).value()));
            countRightSDI(messages.at(i)->senderId(), messages, messageDur, rightSDI, taskCore, T,  data);
        }
    }
    for (int i = 1; i < SchedInt/T; i++){
        rightSDI.find(nextTask).value().append(rightSDI.find(nextTask).value().first()+i*T);
    }
}


double curLoad(const QList<myTreeNode> &myList, const CoreId &core, ScheduleData *data)
{
	double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore == core) {
            //tempLoad += data->partLoad(t.mPart, core);
            tempLoad+=partLoad[core.coreNum][(*iter).mPart.getId()];
        } else {
            continue;
        }
    }
//	foreach (myTreeNode t, myList) {
//		if (t.mCore == core) {
//            //tempLoad += data->partLoad(t.mPart, core);
//            tempLoad+=partLoad[core.coreNum][t.mPart.getId()];
//		} else {
//			continue;
//		}
//	}
	return tempLoad;
}

double connectionWithCore(const QList<myTreeNode> &myList, const CoreId &core, const ObjectId part,ScheduleData *data)
{
    double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore != core) {
            //tempLoad += data->partLoad(t.mPart, core);
            tempLoad+=partPartChannelLoad[part.getId()][(*iter).mPart.getId()];
        } else {
            continue;
        }
    }

    return tempLoad;
}

double connectionWithModule(const QList<myTreeNode> &myList, const CoreId &core, const ObjectId part,ScheduleData *data)
{
    double tempLoad = 0.0;
    QList<myTreeNode>::const_iterator  iter;
    for(iter=myList.begin(); iter!=myList.end();iter++){
        if ((*iter).mCore.moduleId == core.moduleId) {
            //tempLoad += data->partLoad(t.mPart, core);
            tempLoad+=partPartChannelLoad[part.getId()][(*iter).mPart.getId()];
        } else {
            continue;
        }
    }

    return tempLoad;
}


double connectionWithBounded(const QList<ObjectId> &Bounded, const ObjectId part ){
    double d=0;
    foreach(ObjectId o , Bounded){
        d+=partPartChannelLoad[part.getId()][o.getId()];
    }
    return d;
}

bool isplaining()
{
	//TODO  checking for planning tasks with this assignment

	return true;
}

//greedy sort of partitions
//just for speed
QList<ObjectId> gredPartSort(
		QList<ObjectId> partsToBind, ScheduleData *data,
		bool &flag_for_optimize, int &index_for_optimize)
{
	if (partsToBind.size() < 2)
		return partsToBind;

	//map for next choose of the next part. if all comm. are zero
	QMap<ObjectId, double> mapLoad;
	foreach (ObjectId ind, partsToBind) {
		mapLoad[ind] = data->partTotalChannelLoad(ind);
	}
	QList<ObjectId> retList;
	bool null_flag = false;
	while (!partsToBind.isEmpty()) {
		ObjectId optPart;
		double max_load = 0.0;
		//try to find max communicated with solved partitions
		foreach (ObjectId ind, partsToBind) {
			double cur_load = 0.0;
			foreach(ObjectId p, retList)
                cur_load += data->partPartChannelLoad(p, ind);
                //cur_load+=partPartChannelLoad[p.getId()][ind.getId()];
			if (cur_load > max_load) {
				max_load = cur_load;
				optPart = ind;
			}
		}
		//find max communication partition if prev step failed
		if (max_load < EPS) {
			foreach (ObjectId ind, partsToBind) {
				double cur_load = data->partTotalChannelLoad(ind);
				if (cur_load > max_load) {
					max_load = cur_load;
					optPart = ind;
				}
			}
			if (max_load < EPS) {
				optPart = partsToBind[0];
				double temp_max_load = 0.0;
				if (!null_flag) {
					for (QMap<ObjectId, double>::iterator
						ii = mapLoad.begin();
						ii != mapLoad.end();
						++ii)
					{
						if (ii.value() > temp_max_load) {
							optPart = ii.key();
							temp_max_load = ii.value();
						}
					}
                }
				if (temp_max_load < EPS) {
					optPart = partsToBind[0];
					null_flag = true;
					if (!flag_for_optimize) {
						flag_for_optimize = true;
						index_for_optimize = retList.size();
					}
				}
			}
		}
		retList.append(optPart);
		partsToBind.removeAll(optPart);
		mapLoad.remove(optPart);
	}
	return retList;
}

bool finalCondition(const QList<myTreeNode> &current, CoreId core)
{
	foreach (myTreeNode t, current) {
		if (t.mCore != core) {
			return false;
		}
	}
	return true;
}

bool somePartsInModule(const QList<myTreeNode> &Solution, const CoreId cor){
    foreach (myTreeNode t, Solution) {
        if (t.mCore.moduleId == cor.moduleId ) {
            return true;
        }
    }
    return false;
}


double lowerBoundCounting(QList<myTreeNode> &lowerSolution, double &lowerBound,const ObjectIdList &partsToBind,
                          const ObjectIdList &Bounded, Schedule* shed, const QMultiMap<ObjectId, CoreId> &extraConstr,
                          QMap<ObjectId, CoreId> &fixedParts, const QMap<ObjectId, double> &moduleThrConstr ){

     ObjectIdList notBoundedHere;//��������� ���������������� ��������, �� ��� ����� ������ � ���� ������
     double maxD=0;
     //bool flag=true;// ���� ���������� ���������� �������

     //������ ��������� ���������������� ��������
     foreach(ObjectId part, partsToBind){
         if(!Bounded.contains(part))
             notBoundedHere.push_back(part);
     }
     QList<CoreId> availCores;
     ObjectId deleteNow;// ������, ������� � ������ �������� ����� �����������
     CoreId whereToPut;// ����, �� ������� ������ ���� ����� �����������
     // ���� ��������� �������������� �������� �� ����� � ���� ����� ��������� �������
     while(notBoundedHere.length()!=0 ){

         maxD=0;
         //��� ������� ������� �� ��������� ����������������
         //������� ������ ��� ������� ��������
         foreach( ObjectId part, notBoundedHere){
             availCores.clear();
             double max1=0;
             double max2=0;
             //CoreId maxCor;// ���� ���������� ������ ������� ��������
             int maxCor=0;
             ObjectId maxMod;
             // ����, �� ������� ����� ��������� ������ � ������ �� ���������� ��������

             foreach(CoreId cor, shed->data()->coresForPartition(part)){

                 if(fixedParts.contains(part) && fixedParts.find(part).value() != cor){
                     continue;
                 }

                 bool extra = false;
                 QMultiMap<ObjectId, CoreId>::const_iterator pe = extraConstr.find(part);
                 while (pe!=extraConstr.end() && pe.key() == part){
                     if (pe.value()== cor){
                         extra = true;
                         break;
                     }
                     pe++;
                 }
                 if (extra) continue;
                 //double loadCur = curLoad(lowerSolution, cor, shed->data()) + shed->data()->partLoad(part,cor);
                 double loadCur = curLoad(lowerSolution, cor, shed->data()) + partLoad[cor.coreNum][part.getId()];
                 bool thr = countEndThroughtput(lowerSolution,cor, part, shed->data(),moduleThrConstr);
                 //double mcl = shed->constraints().maxCoreLoad.value(cor, 1.0);
                 double mcl= shed->constraints().maxCoreLoad[cor];
                 //���� � ���� ���� ��� ���� �����
                 if(loadCur<=mcl && thr ){
                     availCores.push_back(cor);
                 }
             }

             //���� ��������� ��� ������� ���� ���- ������ ��������� ������� ����
             if(availCores.length()==0){
                 return 0;
             }

             //���� ��������� ������ 1 ����- �������� �� ���� ������ ������, �� ������ ������� ��������
             if(availCores.length()==1){
                 deleteNow=part;
                 whereToPut=availCores[0];
                 break;
             }
             /*bool check;
             if(lowerSolution.count()!=0) check=true;
             else check=false;*/
             //��� ������� ���������� ���� ��� ������� ������� ��������� ������ ��������
             int y=0;
             foreach(CoreId cor, availCores){

                 double f=0;
                 bool check= somePartsInModule(lowerSolution,cor);// ��������, ���� �� ��� � ������, ���������� ������ ����, ���� 1 ������
                // if(shed->data()->partLoad(part,cor)!=0){//�������� �� ������� �� 0
                 if(partLoad[cor.coreNum][part.getId()]!=0){
                    if(check){
                        //���� �  ������ ��� ���� �������, ������� ������� ��� �����, ����������� ����� � ������ �������� �� ��� �������� ����
                       // f=addLinks(lowerSolution, myTreeNode(part,cor), shed->data())/shed->data()->partLoad(part,cor);
                        f=addLinks(lowerSolution, myTreeNode(part,cor), shed->data())/partLoad[cor.coreNum][part.getId()];
                    }
                    else{
                        //���� ������ ����- ������������ �������� ����, ������� �� �������� ���� ��������
                        //f=shed->constraints().maxCoreLoad.value(cor, 1.0)/shed->data()->partLoad(part,cor);
                        f=shed->constraints().maxCoreLoad.value(cor, 1.0)/partLoad[cor.coreNum][part.getId()];
                    }
                 }
                 if(f>=max1){
                     max2=max1;
                     max1=f;
                     maxCor=y;
                     //maxMod=cor.moduleId;
                 }
                 else if (f>=max2){
                     max2=f;
                 }
                 y++;
             }
             //���� ����������� ������� ������ ���� ������� �������� �� ���� ������������� ��������
             if(max1-max2>=maxD){
                 maxD=max1-max2;
                 //CoreId ct=shed->data()->mod
                 whereToPut=availCores.at(maxCor);
                 deleteNow=part;
             }
         }
         //������� �� ���������������� ������ � ���������� �������� ��������
         notBoundedHere.removeOne(deleteNow);
         //��������� ���� ������ � ����, ���������� ������������ ������� �������� ������� � ��������� ��� ��� � ������� �������
         myTreeNode newOne(deleteNow,whereToPut);
         lowerSolution.push_back( newOne);
         // ����� ���������� � �������� ������� �����, �������� ����������� ��������
         lowerBound+=addLinks(lowerSolution, newOne,shed->data());
     }
     QMap <ObjectId, double> mesDur ;
     QMultiMap<ObjectId, CoreId> parts;
     QMap<ObjectId, double> mesConstr;
    if (PDCChecker::checkPDC(mesDur, lowerSolution, shed->data(), true, parts, fixedParts, mesConstr )){
        return lowerBound;
    }
return 0;
}

double ModuleLoad(const Module*mod,const QList<ObjectId> &BoundedInMod, Schedule* shed){
    double d=0;
    //CoreId c=mod->cores().at(0);
    CoreId c;
    QList<CoreId>::const_iterator core;
    for(core=shed->data()->cores().begin();core!=shed->data()->cores().end(); core++){
        if ((*core).moduleId==mod->id()){
            c=(*core);
            break;
        }
    }
//    foreach(CoreId core, shed->data()->cores()){
//        if (core.moduleId==mod->id()){
//            c=core;
//            break;
//        }
//    }

    QList<ObjectId>::const_iterator o;
    for(o=BoundedInMod.begin();o!=BoundedInMod.end();o++){
         d+=partLoad[c.coreNum][(*o).getId()];
    }
//    foreach(ObjectId o, BoundedInMod){
//        //d+=shed->data()->partLoad(o,c);
//        d+=partLoad[c.coreNum][o.getId()];
//    }
    return d;
}

double AllLinks(const ObjectId part,const ObjectIdList &partsToBind,  Schedule* shed){
    double d=0;
    foreach(ObjectId o, partsToBind){
        if(o!=part){
            //d+=shed->data()->partPartChannelLoad(o,part);
            d+=partPartChannelLoad[o.getId()][part.getId()];
        }
    }
    return d;
}

double CountBound(const Module*mod,const double maxModLoad,const QList<myTreeNode> &ModuleSolution,const QList<ObjectId> &Bounded,
                  const ObjectId BannedPartsInMod,const QList<ObjectId>& BoundedInMod,QList<ObjectId> & BoundedInModNew,
                  QMap<ObjectId,int> &M0M2, Schedule* shed, const bool flag,const ObjectIdList &partsToBind,const double Zn0){
    QList<ObjectId> BoundedHere(BoundedInMod);
    QList<myTreeNode> curModSolution(ModuleSolution);// ���������� ��� �������� ������� ������
    QList<ObjectId> availParts;// ���� ��������� ��� ������� ������ ��������
    CoreId c;
    foreach(CoreId core, shed->data()->cores()){
        if (core.moduleId==mod->id()){
            c=core;
            break;
        }
    }

    double place=ModuleLoad(mod,BoundedInMod,shed);
    double Zn=Zn0;// ��������� �������
    bool ready=false;// ����, tckb true- ������ ������ ������ ��������� � ������
    foreach(ObjectId p, partsToBind){//��� ������� ������� �� ������ ���� ��������
        // ���� ������ ��� �� ���������� � ������ � �� �������� ��� ����� ������ � ���� ������ ��������� �������� ��� ������� ������
        if(!Bounded.contains(p)&&!BoundedInMod.contains(p)&&BannedPartsInMod!=p&&shed->data()->modulesForPartition(p).contains(mod->id())){
            //� �����, ���� � ������ �������� ����� ��� ����� �������
           // if(ModuleLoad(mod,BoundedInMod,shed)+shed->data()->partLoad(p,c)<=maxModLoad)
            if(place+partLoad[c.coreNum][p.getId()]<=maxModLoad)
                availParts.push_back(p);// ������� ���� ������ � ���� ��������� ������� ������
        }
    }

    //���� ���� ��������� ������� � ���� 1 ������ ��� ���������� �� ������
    while(!ready&& availParts.count()!=0){
        double max=0;//�������� ������� ��������
        ObjectId put;// ������, ��������������� ��������� ��������
        foreach(ObjectId o, availParts){// ��� ������� ������� �� ���������
            //���� � ������ �������� ����� ��� ������� �������
            //if(ModuleLoad(mod,BoundedHere,shed)+shed->data()->partLoad(o,c)<=maxModLoad){
            if(place+partLoad[c.coreNum][o.getId()]<=maxModLoad){
                double d;// �������� ��������
                //double w=shed->data()->partLoad(o,c);// �������� ������� ������ ������ ��������
                double w=partLoad[c.coreNum][o.getId()];
                if(w!=0){
                    myTreeNode t(o,c);
                    if(BoundedHere.count()==0){//���� � ������ ������ ��� ������ �� �����
                        d=AllLinks(o,availParts,shed)/w;// ��� ����� ������� �������, �������� �� ��� �������� ������
                        //d-=shed->data()->partPartChannelLoad(o,BannedPartsInMod);
                        d-=partPartChannelLoad[o.getId()][BannedPartsInMod.getId()];
                    }
                    else{
                        d=addLinks(curModSolution,t,shed->data())/w;// ����� ������� � �������������� ������, �������� �� ��������
                    }
                    if(d>=max) {// ������������� ��������
                        max=d;
                        put=o;
                    }
                }
            }
        }
        if(max!=0){// ���� �������� �� 0
            availParts.removeOne(put);// ������ ������, ��������������� ��������� �� ���������
            Zn+=addLinks(curModSolution,myTreeNode(put,c),shed->data());// ������� ����� ����� ������� � ���������� �������
            curModSolution.push_back(myTreeNode(put,c ));// ������� ���� ������ � ������
            BoundedHere.push_back(put);
            place+=partLoad[c.coreNum][put.getId()];

            if(flag) {// ���� ���� ��������� ������� �������
                BoundedInModNew.push_back(put);// ������� ������ � ���� �������������� � ������ ��������
                M0M2[put]+=1;// ������� 1 � ����� ����������� ������� ������� �� ���� �������
            }
        }
        else ready=true;// ���� ����= 0 - ��� , ��� ����� ���� �������� �� ��������
    }
    return Zn;
}

double UpperBoundCounting(const QList<myTreeNode> &currentSolution,const ObjectIdList &partsToBind,
                          const ObjectIdList  &Bounded, Schedule* shed){
    QMap<ObjectId, double> newCores;// ��� ������� � �� ������������ �������� ��������
    QMap<ObjectId,int> M0M2;// ��� �������� �0 � �2+( ��� ������� ������� ������������ ������� ��� �� ����� � ������� ��������)
    QList<myTreeNode> curModuleSolution[shed->data()->modules().count()];// ����� � ��������� ������� ���������� ������
    QList<ObjectId> BoundedInMod[shed->data()->modules().count()];//����� � ���������, ������� ��������� � ������ ������ � ������� �������
    QList<ObjectId> BoundedInModNew[shed->data()->modules().count()];//����� � ���������, ������� ��������� � ������ ����� ������� ���������� �������
    ObjectId BannedPartsInMod[shed->data()->modules().count()];//����������� ��� ������ ������
    QMap<Module*,double> Zn0;// MAP � ������� � ��� �������� �� ���������
    foreach(ObjectId o, partsToBind){
        if(!Bounded.contains(o)){
            M0M2[o]=0;//��������
        }
    }

    int i=0;
    foreach(Module* mod,shed->data()->modules()){// ��� ������� ������ �������- ������ MAP � ������� � �������� �����
        double d=0;
        Zn0[mod]=0;
        foreach(CoreId c,mod->cores()){
            d+=shed->constraints().maxCoreLoad[c];// ������� ��� ���� � ����� ������
        }
        newCores[mod->id()]=d;
        foreach(myTreeNode temp, currentSolution){//��� ������� ���� � ������� �������
            if(temp.mCore.moduleId==mod->id()){// ���� ���� ����� ���� ��������� � ������ ������
                BoundedInModNew[i].push_back(temp.mPart);
                BoundedInMod[i].push_back(temp.mPart);// ������� ������ ����� ���� � ���� � ������� ����� ������
                Zn0[mod]+= addLinks(curModuleSolution[i],temp , shed->data() );// �������� ����� ����� ����
                curModuleSolution[i].push_back(temp);// ������� ���� ���� � ������� ������� ������
            }
        }
        i++;
    }

    bool flag=true; //����: ���� �� � �������� ������� �������� BoundedInModNew � �0�2; ��� ������ ������� ��� Zn �� �������� ����- �� �� ��� ������ ������
    // ��� ���������� u1 � u0 �� �������� �� ���� - �������� ���.
    QMap<Module*,double> Zn;// MAP � ������� � ��� �������� �� ������ ���� ���������
    double U0=0;
    i=0;
    foreach(Module* mod,shed->data()->modules()){// ��� ������� ������ ������� ��� ��������� �������
        Zn[mod]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);
        U0+=Zn[mod];// ���������� ��� ������� � ������ ������� �������
        i++;
    }
    QList<ObjectId> M0;// ��������� �0
    QList<ObjectId> M2;// ��������� �2+
    foreach(ObjectId m, M0M2.keys()){// ��� ������� ������� ����� ���������
        if(M0M2[m]==0) M0.push_back(m);// ���� ������ ����� 0 ��� � �������- ������� ��� � �0
        else if(M0M2[m]>1) M2.push_back(m);// ���� ������ 1 ����- ������� ��� � �2+
    }
    double maxZn=0;
    foreach(Module* m, shed->data()->modules()){
        if(Zn[m]>maxZn)maxZn=Zn[m];// ������ ������������ Zn
    }

    flag=false;
    double GlobalMaxD=0;// ��, ��� ����� �������� �� U0
    double D[M2.count()+M0.count()];// ������ ��� D ��� ������� �� �������� M0 � �2+
    double u1[M0.count()][shed->data()->modules().count()];// u1 ��� ������� ������� M0 � ���� ��������
    int j=0;
    foreach(ObjectId o, M0){// ��� ������� �� �0 ������� u1 � ������� ����������� ������
         i=0;
         double minD=maxZn;
         foreach(Module* mod,shed->data()->modules()){// ���������� �������� � ������ ������ ������� ������ �� �0
            // ���������, ���� �� ������ ����� ��� ����� ������� � ������ ������ � �������� �� ���� ������ ��� �������
            //if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+shed->data()->partLoad(o,mod->cores()[0])<=newCores[mod->id()]){
             if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+partLoad[mod->cores()[0].coreNum][o.getId()]<=newCores[mod->id()]){
             // ������������� �������� �����
                BoundedInMod[i].push_back(o);
                curModuleSolution[i].push_back(myTreeNode(o,mod->cores()[0]));// ����� ������������ ���� �� ������
                u1[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// ������������� �������
                BoundedInMod[i].pop_back();// ���������� ��� �����
                curModuleSolution[i].pop_back();
            }
            else{
                 u1[j][i]=0;
            }
            if(Zn[mod]<u1[j][i])u1[j][i]=Zn[mod];
            if(Zn[mod]-u1[j][i]<minD) minD=Zn[mod]-u1[j][i]<minD;// �������� ����������� �������
            i++;
        }
        D[j]=minD;// � �������� D ��� ����� ������� ����� ����������� �������
        if(D[j]>GlobalMaxD) GlobalMaxD=D[j];// �������� ������������� D
        j++;
    }

    double u0[M2.count()][shed->data()->modules().count()];// u0 ��� ������� ������� �� �2+ � ������� ������, ���� ������ ���� ������

     j=0;

    foreach(ObjectId o, M2){//��� ������� ������� �� �2+
        double Sum=0;//������� ����� ���� ������
        double maxD=0;// ������� ������������ �������
         i=0;
        foreach(Module* mod,shed->data()->modules()){//��� ������� ������
            if(BoundedInModNew[i].contains(o)){// ���� ���� ������ ������� � ������
                BannedPartsInMod[i]=o;
                //BannedPartsInMod[i].push_back(o);// �������� ���� ������ � ���� ������
                u0[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// ����������� ������� ����� ������
                //BannedPartsInMod[i].pop_back();// ������ ������ �� �����������
                if(Zn[mod]<u0[j][i])u0[j][i]=Zn[mod];
                Sum+=Zn[mod]-u0[j][i];// ������� � ����� ��������
                if(Zn[mod]-u0[j][i]>maxD) maxD=Zn[mod]-u0[j][i];// ����������� �������� �������
            }
            i++;
        }
        D[M0.count()+j]=Sum-maxD;//��������� D ��� ������� �������
        if(D[M0.count()+j]>GlobalMaxD) GlobalMaxD=D[M0.count()+j];// ����������� �������� D
        j++;
    }
  return U0-GlobalMaxD;
}

//int AntipBinding(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
//                 const ObjectIdList &partsToBind, ObjectIdList &Bounded,const QList<CoreId> &cores,Schedule* shed, int iter ){
//    QList<myTreeNode> lowerSolution(currentBranch);// ������ ������� ( ���������� � ��� ����� ����������� ��� ���� �������� �������)
//    double upperBound=0;// ����� ������� ������� �������
//    double lowerBound=curDoubleSol;// ����� ������� ������ �������( � ���� ����� ������ ����� �������� �������)
//    if(MaxPosLoad-optDoubleSol>=reversCurSol){
//        if(iter == partsToBind.length()){//�������� ���� ����� --��������� �����-�� �������, �������� ��������� ��� ������
//            if(curDoubleSol>optDoubleSol) {
//                optDoubleSol=curDoubleSol;
//                optSolution.clear();
//                foreach (myTreeNode t, currentBranch) {
//                   optSolution.push_back(t);
//                }
//            }
//            return 1;
//        }
//    //qDebug()<<iter;
//        upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//������� ������� �������-- �������� ���������
//        if(upperBound<=optDoubleSol){
//            return 0;
//        }

//        QMultiMap<ObjectId, CoreId> ep;
//        lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//������� ������ �������
//        if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//�������� �� �������������
//            optDoubleSol=lowerBound;
//            optSolution.clear();
//            foreach (myTreeNode t, lowerSolution) {
//               optSolution.push_back(t);
//            }
//        }
//        if(optDoubleSol==MaxPosLoad){return 1;}
//     // ������� ������ � �������� iter+1 � ������ ��������������
//        //Bounded.push_back(partsToBind[iter+1]);
//        Bounded.push_back(partsToBind.at(iter));//+1
//        //��� ������� ���������� ���� ��� ���������� �������

//        foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//            //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
//            double loadCur = curLoad(currentBranch, c,  shed->data()) + partLoad[c.coreNum][partsToBind[iter].getId()];
//            double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//            //���� � ���� ���� ��� ���� �����

//            if(loadCur<=mcl){

//                //�������� ��������� ������ � ��� ����,

//               // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                myTreeNode temp(partsToBind.at(iter),c);//+1
//                currentBranch.push_back(temp);

//                //������� � ������� ������� �����, ������ ���� ���� ������
//                double a=addLinks(currentBranch, temp, shed->data());
//                double b=addTime(currentBranch, temp, shed->data());

//                curDoubleSol+=a;
//                reversCurSol+=b;

//                //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
//                // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
//                int f= AntipBinding(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1);

//                //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
//                currentBranch.pop_back();

//                // ������ ����� ������� �������
//                curDoubleSol-=a;
//                reversCurSol-=b;
//                //Bounded.pop_back();
//            }
//        }

//        Bounded.pop_back();

//        return 1;
//     }
// return 0;
// }

//****
//* ���������� ������� �� ���� ��������� ������� ������
//* ���� U0-maxDi
//* ���� U0-sumDi/2
//****
double UpperBoundCountingUpdate(const QList<myTreeNode> &currentSolution,const ObjectIdList &partsToBind,
                          const ObjectIdList  &Bounded, Schedule* shed){
    QMap<ObjectId, double> newCores;// ��� ������� � �� ������������ �������� ��������
    QMap<ObjectId,int> M0M2;// ��� �������� �0 � �2+( ��� ������� ������� ������������ ������� ��� �� ����� � ������� ��������)
    QList<myTreeNode> curModuleSolution[shed->data()->modules().count()];// ����� � ��������� ������� ���������� ������
    QList<ObjectId> BoundedInMod[shed->data()->modules().count()];//����� � ���������, ������� ��������� � ������ ������ � ������� �������
    QList<ObjectId> BoundedInModNew[shed->data()->modules().count()];//����� � ���������, ������� ��������� � ������ ����� ������� ���������� �������
    ObjectId BannedPartsInMod[shed->data()->modules().count()];//����������� ��� ������ ������
    QMap<Module*,double> Zn0;// MAP � ������� � ��� �������� �� ���������
    foreach(ObjectId o, partsToBind){
        if(!Bounded.contains(o)){
            M0M2[o]=0;//��������
        }
    }

    int i=0;
    foreach(Module* mod,shed->data()->modules()){// ��� ������� ������ �������- ������ MAP � ������� � �������� �����
        double d=0;
        Zn0[mod]=0;
        foreach(CoreId c,mod->cores()){
            d+=shed->constraints().maxCoreLoad[c];// ������� ��� ���� � ����� ������
        }
        newCores[mod->id()]=d;
        foreach(myTreeNode temp, currentSolution){//��� ������� ���� � ������� �������
            if(temp.mCore.moduleId==mod->id()){// ���� ���� ����� ���� ��������� � ������ ������
                BoundedInModNew[i].push_back(temp.mPart);
                BoundedInMod[i].push_back(temp.mPart);// ������� ������ ����� ���� � ���� � ������� ����� ������
                Zn0[mod]+= addLinks(curModuleSolution[i],temp , shed->data() );// �������� ����� ����� ����
                curModuleSolution[i].push_back(temp);// ������� ���� ���� � ������� ������� ������
            }
        }
        i++;
    }

    bool flag=true; //����: ���� �� � �������� ������� �������� BoundedInModNew � �0�2; ��� ������ ������� ��� Zn �� �������� ����- �� �� ��� ������ ������
    // ��� ���������� u1 � u0 �� �������� �� ���� - �������� ���.
    QMap<Module*,double> Zn;// MAP � ������� � ��� �������� �� ������ ���� ���������
    double U0=0;
    i=0;
    foreach(Module* mod,shed->data()->modules()){// ��� ������� ������ ������� ��� ��������� �������
        Zn[mod]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);
        U0+=Zn[mod];// ���������� ��� ������� � ������ ������� �������
        i++;
    }
    QList<ObjectId> M0;// ��������� �0
    QList<ObjectId> M2;// ��������� �2+
    foreach(ObjectId m, M0M2.keys()){// ��� ������� ������� ����� ���������
        if(M0M2[m]==0) M0.push_back(m);// ���� ������ ����� 0 ��� � �������- ������� ��� � �0
        else if(M0M2[m]>1) M2.push_back(m);// ���� ������ 1 ����- ������� ��� � �2+
    }
    double maxZn=0;
    foreach(Module* m, shed->data()->modules()){
        if(Zn[m]>maxZn)maxZn=Zn[m];// ������ ������������ Zn
    }

    flag=false;
    double GlobalMaxD=0;// ��, ��� ����� �������� �� U0
    double SumD=0;
    double D[M2.count()+M0.count()];// ������ ��� D ��� ������� �� �������� M0 � �2+
    double u1[M0.count()][shed->data()->modules().count()];// u1 ��� ������� ������� M0 � ���� ��������
    int j=0;
    foreach(ObjectId o, M0){// ��� ������� �� �0 ������� u1 � ������� ����������� ������
         i=0;
         double minD=maxZn;
         foreach(Module* mod,shed->data()->modules()){// ���������� �������� � ������ ������ ������� ������ �� �0
            // ���������, ���� �� ������ ����� ��� ����� ������� � ������ ������ � �������� �� ���� ������ ��� �������
            //if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+shed->data()->partLoad(o,mod->cores()[0])<=newCores[mod->id()]){
             if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+partLoad[mod->cores()[0].coreNum][o.getId()]<=newCores[mod->id()]){
             // ������������� �������� �����
                BoundedInMod[i].push_back(o);
                curModuleSolution[i].push_back(myTreeNode(o,mod->cores()[0]));// ����� ������������ ���� �� ������
                u1[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// ������������� �������
                BoundedInMod[i].pop_back();// ���������� ��� �����
                curModuleSolution[i].pop_back();
            }
            else{
                 u1[j][i]=0;
            }
            if(Zn[mod]<u1[j][i])u1[j][i]=Zn[mod];
            if(Zn[mod]-u1[j][i]<minD) minD=Zn[mod]-u1[j][i]<minD;// �������� ����������� �������
            i++;
        }
        D[j]=minD;// � �������� D ��� ����� ������� ����� ����������� �������
        SumD+=D[j];
        if(D[j]>GlobalMaxD) GlobalMaxD=D[j];// �������� ������������� D
        j++;
    }

    double u0[M2.count()][shed->data()->modules().count()];// u0 ��� ������� ������� �� �2+ � ������� ������, ���� ������ ���� ������

     j=0;

    foreach(ObjectId o, M2){//��� ������� ������� �� �2+
        double Sum=0;//������� ����� ���� ������
        double maxD=0;// ������� ������������ �������
         i=0;
        foreach(Module* mod,shed->data()->modules()){//��� ������� ������
            if(BoundedInModNew[i].contains(o)){// ���� ���� ������ ������� � ������
                BannedPartsInMod[i]=o;
                //BannedPartsInMod[i].push_back(o);// �������� ���� ������ � ���� ������
                u0[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// ����������� ������� ����� ������
                //BannedPartsInMod[i].pop_back();// ������ ������ �� �����������
                if(Zn[mod]<u0[j][i])u0[j][i]=Zn[mod];
                Sum+=Zn[mod]-u0[j][i];// ������� � ����� ��������
                if(Zn[mod]-u0[j][i]>maxD) maxD=Zn[mod]-u0[j][i];// ����������� �������� �������
            }
            i++;
        }
        D[M0.count()+j]=Sum-maxD;//��������� D ��� ������� �������
        SumD+=D[M0.count()+j];
        if(D[M0.count()+j]>GlobalMaxD) GlobalMaxD=D[M0.count()+j];// ����������� �������� D
        j++;
    }
    double SumD2=SumD/2;
    if(GlobalMaxD>SumD2) return U0-GlobalMaxD;
    else{  return U0-SumD2;}
}

 bool compareCores(CoreId c1, CoreId c2, Schedule* shed){
     double x =shed->constraints().maxCoreLoad[c1];
     double y=shed->constraints().maxCoreLoad[c2];
     double b= x-y;
     double a=fabs(shed->constraints().maxCoreLoad[c1]-shed->constraints().maxCoreLoad[c2]);
        return c1.moduleId==c2.moduleId &&
                c1.processorTypeId()== c2.processorTypeId() &&
                (fabs(shed->constraints().maxCoreLoad[c1]-shed->constraints().maxCoreLoad[c2])<=0.000001);
 }

 bool psevdoCompareCores(PsevdoGroup& group,CoreId c1,Schedule * shed){
     CoreId c2=group.groupList.at(0);
     if(group.volums.size()==1) return c1.moduleId==c2.moduleId &&
             c1.processorTypeId()== c2.processorTypeId() &&
             (fabs(shed->constraints().maxCoreLoad[c1]-shed->constraints().maxCoreLoad[c2])<=2*DELTA);
     if(group.volums.size()==2){
         if(fabs(group.volums.values().at(0)-group.volums.values().at(1))<=DELTA){
             return c1.moduleId==c2.moduleId &&
                    c1.processorTypeId()== c2.processorTypeId() &&
                    ((fabs(shed->constraints().maxCoreLoad[c1]-group.volums.values().at(0))<=DELTA) ||
                     (fabs(shed->constraints().maxCoreLoad[c1]-group.volums.values().at(1))<=DELTA));
         }
         else
             return c1.moduleId==c2.moduleId &&
                    c1.processorTypeId()== c2.processorTypeId() &&
                    ((fabs(shed->constraints().maxCoreLoad[c1]-group.volums.values().at(0))<=DELTA) &&
                     (fabs(shed->constraints().maxCoreLoad[c1]-group.volums.values().at(1))<=DELTA));
     }
     return c1.moduleId==c2.moduleId &&
             c1.processorTypeId()== c2.processorTypeId() &&
             (fabs(shed->constraints().maxCoreLoad[c1]-group.midVol)<=DELTA);
 }

 void recountGroup(PsevdoGroup& group,double l){
     if(group.volums.size()==3){
         double max= group.volums.values().at(0);
         double min= group.volums.values().at(0);
         for(int i=1;i<3;i++){
             if(group.volums.values().at(i)>max){
                 max= group.volums.values().at(i);
             }
             if(group.volums.values().at(i)<min){
                 min= group.volums.values().at(i);
             }
         }
         group.midVol=(max-min)/2;
     }
     if(l<group.groupVol) group.groupVol=l;
     if(group.volums.size()==2){
         if(((l<group.volums.values().at(0))&&(l>group.volums.values().at(1)))||
                 ((l>group.volums.values().at(0))&&(l<group.volums.values().at(1)))){
             group.groupVol=l;
         }
     }
     group.volums.insert(l);
 }

 QMultiHash<ObjectId,Group> initCoreGroups(QList<CoreId> cores, Schedule* shed, const QMultiMap<ObjectId, CoreId> &extraConstr ){
     QMultiHash<ObjectId,Group> groups;
     int num=0;
     for(int i =0; i<= cores.length()-1;i++){
         bool extra = false;
         QMultiMap<ObjectId, CoreId>::const_iterator pe = extraConstr.cbegin();
         while (pe!=extraConstr.end()){
             if (pe.value()== cores.at(i)){
                 extra = true;
                 break;
             }
             pe++;
         }
         if(extra) continue;
         if(shed->data()->coreLoad(cores.at(i))<=0.0){
            bool groupEx= false;
            QMultiHash<ObjectId, Group>::iterator iter =groups.find(cores.at(i).moduleId);
            while(iter!=groups.end() && iter.key()==cores.at(i).moduleId){
                if(iter.value().groupList.contains(cores.at(i))) {
                    groupEx=true;
                    break;
                }
                iter++;
            }
            if(!groupEx){
                for(int j=i+1; j<cores.length();j++){
                    if(shed->data()->coreLoad(cores.at(j))<=0.0){
                        if(compareCores(cores.at(i),cores.at(j), shed)){
                            if(!groupEx){
                                //������� ������ � �������� ��� �������
                                Group grStruct;
                                grStruct.num=num;
                                num++;
                                grStruct.groupList.append(cores.at(i));
                                grStruct.groupList.append(cores.at(j));
                                groups.insert(cores.at(i).moduleId,grStruct);
                                groupEx=true;
                            }
                            else{
                                //�������� ������ ���������� ������
                                QMultiHash<ObjectId,Group>::iterator iter =groups.find(cores.at(i).moduleId);
                                while(iter!=groups.end() && iter.key()==cores.at(i).moduleId){
                                    if(iter.value().groupList.contains(cores.at(i))) {
                                        iter.value().groupList.append(cores.at(j));
                                        break;
                                    }
                                    iter++;
                                }
                            }
                        }
                    }
                }
            }
         }
     }
     return groups;
 }

 QMultiHash<ObjectId,PsevdoGroup> initCorePsevdoGroups(QList<CoreId>& cores, Schedule* shed){
     QMultiHash<ObjectId,PsevdoGroup> groups;
     int num=0;
     for(int i =0; i<= cores.length()-1;i++){
         if(shed->data()->coreLoad(cores.at(i))<=0.0){
            bool exGroup= false;
            bool checked=false;
            QMultiHash<ObjectId, PsevdoGroup>::iterator iter =groups.find(cores.at(i).moduleId);
            while(iter!=groups.end() && iter.key()==cores.at(i).moduleId){
                if(iter.value().groupList.contains(cores.at(i))){
                    checked=true;
                    break;
                }
                iter++;
            }
            if(!checked){
                iter=groups.find(cores.at(i).moduleId);
                while(iter!=groups.end() && iter.key()==cores.at(i).moduleId){
                    exGroup=psevdoCompareCores(iter.value(),cores.at(i),shed);
                    if(exGroup){
                        iter.value().groupList.append(cores.at(i));
                        recountGroup(iter.value(),shed->constraints().maxCoreLoad[cores.at(i)]);
                        for(int j=i+1;j<cores.length()-1;j++){
                            if(compareCores(cores.at(i),cores.at(j),shed)){
                                iter.value().groupList.append(cores.at(j));
                            }
                        }
                        break;
                    }
                    iter++;
                }
            }
            if(!exGroup&&!checked){
                //������� ������ � �������� ������
                PsevdoGroup grStruct;
                grStruct.num=num;
                num++;
                grStruct.groupList.append(cores.at(i));
                grStruct.volums.insert(shed->constraints().maxCoreLoad[cores.at(i)]);
                grStruct.groupVol=shed->constraints().maxCoreLoad[cores.at(i)];
                for(int j=i+1;j<=cores.length()-1;j++){
                    if(compareCores(cores.at(i),cores.at(j),shed)){
                        grStruct.groupList.append(cores.at(j));
                    }
                }
                groups.insert(cores.at(i).moduleId,grStruct);
            }
         }
     }
     return groups;
 }

 int AntipBindingWithGroups(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
                  const ObjectIdList partsToBind, ObjectIdList Bounded,const QList<CoreId> &cores,Schedule* shed, int iter,
                            QMultiHash<ObjectId,Group> &groups,const QMultiMap<ObjectId, CoreId> &extraConstr,
                            QMap<ObjectId, CoreId> &fixedParts, const QMap<ObjectId, double> &moduleThrConstr){
     QList<myTreeNode> lowerSolution(currentBranch);// ������ ������� ( ���������� � ��� ����� ����������� ��� ���� �������� �������)
     double upperBound=0;// ����� ������� ������� �������
     double lowerBound=curDoubleSol;// ����� ������� ������ �������( � ���� ����� ������ ����� �������� �������)
     if(MaxPosLoad-optDoubleSol>=reversCurSol){
         bool groupFlags[groups.values().length()];
         for(int l=0;l<groups.values().length() ;l++){
             groupFlags[l]=false;
         }
         if(iter == partsToBind.length()){//�������� ���� ����� --��������� �����-�� �������, �������� ��������� ��� ������
             QMap <ObjectId, double> mesDur ;
             QMultiMap<ObjectId, CoreId> parts;
             QMap<ObjectId, double> mesConstr;
             if((curDoubleSol>=optDoubleSol) && (PDCChecker::checkPDC(mesDur, currentBranch, shed->data(), true, parts, fixedParts, mesConstr ))) {
                 optDoubleSol=curDoubleSol;
                 optSolution.clear();
                 foreach (myTreeNode t, currentBranch) {
                    optSolution.push_back(t);
                 }
             }
             return 1;
         }
     //qDebug()<<iter;
         upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//������� ������� �������-- �������� ���������
         if(upperBound<=optDoubleSol && optDoubleSol>0.0){
             return 0;
         }
         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, extraConstr, fixedParts, moduleThrConstr);//������� ������ �������
         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//�������� �� �������������
             optDoubleSol=lowerBound;
             optSolution.clear();
             foreach (myTreeNode t, lowerSolution) {
                optSolution.push_back(t);
             }
         }
         if(optDoubleSol==MaxPosLoad){
             return 1;
         }
      // ������� ������ � �������� iter+1 � ������ ��������������
         //Bounded.push_back(partsToBind[iter+1]);
         Bounded.push_back(partsToBind.at(iter));//+1
         //��� ������� ���������� ���� ��� ���������� �������

         foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1

             if(fixedParts.contains(partsToBind[iter]) && fixedParts.find(partsToBind[iter]).value() != c){
                 continue;
             }

             bool extra = false;
             QMultiMap<ObjectId, CoreId>::const_iterator pe = extraConstr.find(partsToBind[iter]);
             while (pe!=extraConstr.end() && pe.key() == partsToBind[iter]){
                 if (pe.value()== c){
                     extra = true;
                     break;
                 }
                 pe++;
             }
             if (extra) continue;
             bool checked = false;
             bool isThrought = countEndThroughtput(currentBranch, c, partsToBind[iter], shed->data(), moduleThrConstr);
             QMultiHash<ObjectId, Group>::iterator iterat =groups.find(c.moduleId);
             while(iterat!=groups.end() && iterat.key()==c.moduleId){
                 if(iterat.value().groupList.contains(c)) {
                    if(!groupFlags[iterat.value().num]){
                        //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
                        double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
                        double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

                        //���� � ���� ���� ��� ���� �����

                        if((isThrought) && (loadCur<=mcl)){
                            iterat.value().groupList.removeOne(c);
                            //�������� ��������� ������ � ��� ����,

                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
                            myTreeNode temp(partsToBind.at(iter),c);//+1
                            currentBranch.push_back(temp);

                            //������� � ������� ������� �����, ������ ���� ���� ������
                            double a=addLinks(currentBranch, temp, shed->data());
                            double b=addTime(currentBranch, temp, shed->data());

                            curDoubleSol+=a;
                            reversCurSol+=b;


                            //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
                            // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
                            int f= AntipBindingWithGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups, extraConstr, fixedParts,moduleThrConstr);

                            //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
                            currentBranch.pop_back();

                            // ������ ����� ������� �������

                            curDoubleSol-=a;
                            reversCurSol-=b;
                            //Bounded.pop_back();

                            iterat.value().groupList.append(c);
                        }
                        groupFlags[iterat.value().num]=true;
                        checked=true;

                        break;
                     }
                     checked=true;
                 }
                 iterat++;
             }

             if(!checked){
                 //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
                 double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
                 double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

                 //���� � ���� ���� ��� ���� �����

                 if((isThrought) && (loadCur<=mcl)){

                     //�������� ��������� ������ � ��� ����,

                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
                     myTreeNode temp(partsToBind.at(iter),c);//+1
                     currentBranch.push_back(temp);

                     //������� � ������� ������� �����, ������ ���� ���� ������

                     double a=addLinks(currentBranch, temp, shed->data());
                     double b=addTime(currentBranch, temp, shed->data());

                     curDoubleSol+=a;
                     reversCurSol+=b;

                     //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
                     // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
                     int f= AntipBindingWithGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups, extraConstr, fixedParts, moduleThrConstr);

                     //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
                     currentBranch.pop_back();

                     // ������ ����� ������� �������
                     curDoubleSol-=a;
                     reversCurSol-=b;
                     //Bounded.pop_back();
                 }
            }
         }

         Bounded.pop_back();

         return 1;
     }
 return 0;
 }

//****
//* ������� ������� ������� ����� �������
//* ����� ����������������� � ����������� ����� ������� ������� �������
//* �������� ������� ��������� �������� ���������
//* �� ������� ���� ����������� ������ �� �������������
//****
// int AntipBindingWithGroupsUpdate(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
//                  const ObjectIdList partsToBind, ObjectIdList Bounded,const QList<CoreId> &cores,Schedule* shed, int iter,
//                            QMultiHash<ObjectId,Group> &groups){
//     QList<myTreeNode> lowerSolution(currentBranch);// ������ ������� ( ���������� � ��� ����� ����������� ��� ���� �������� �������)
//     double upperBoundUpdate=0;// ����� ������� ������� �������
//     //double upperBound=0;
//     double lowerBound=curDoubleSol;// ����� ������� ������ �������( � ���� ����� ������ ����� �������� �������)
//     if(MaxPosLoad-optDoubleSol>=reversCurSol){
//         bool groupFlags[groups.values().length()];
//         for(int l=0;l<groups.values().length() ;l++){
//             groupFlags[l]=false;
//         }
//         if(iter == partsToBind.length()){//�������� ���� ����� --��������� �����-�� �������, �������� ��������� ��� ������
//             if(curDoubleSol>=optDoubleSol) {
//                 optDoubleSol=curDoubleSol;
//                 optSolution.clear();
//                 //newBoundStream<<"opt solution got\n";
//                // qDebug()<<"opt solution got";
//                 foreach (myTreeNode t, currentBranch) {
//                    optSolution.push_back(t);
//                 }
//             }
//             return 1;
//         }
//     //qDebug()<<iter;
//        // upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);
//         upperBoundUpdate=UpperBoundCountingUpdate(currentBranch,partsToBind,Bounded, shed);//������� ������� �������-- �������� ���������
//         //newBoundStream<<iter<<" update "<<upperBoundUpdate<<" not update "<<upperBound<<"\n";
//         //qDebug()<<iter<<" update "<<upperBoundUpdate<<" not update "<<upperBound<<"\n";
//         if(upperBoundUpdate<=optDoubleSol){
//             //newBoundStream<<"cutting branch by upgrade\n";
//           // qDebug()<<"cutting branch by upgrade\n";
//         //}
//       //  if(upperBound<=optDoubleSol){
//             //newBoundStream<<"cutting branch by old\n";
//             //qDebug()<<"cutting branch by old\n";
//             return 0;
//         }
//         QMultiMap<ObjectId, CoreId> ep;
//         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//������� ������ �������
//         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//�������� �� �������������
//             optDoubleSol=lowerBound;
//             optSolution.clear();
//             //newBoundStream<<"opt solution by lower bound got";
//             //qDebug()<<"opt solution by lower bound got";
//             foreach (myTreeNode t, lowerSolution) {
//                optSolution.push_back(t);
//             }
//         }
//         if(optDoubleSol==MaxPosLoad){
//             return 1;
//         }
//      // ������� ������ � �������� iter+1 � ������ ��������������
//         //Bounded.push_back(partsToBind[iter+1]);
//         Bounded.push_back(partsToBind.at(iter));//+1
//         //��� ������� ���������� ���� ��� ���������� �������

//         foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//             bool checked = false;
//             bool isThrought = countEndThroughtput(currentBranch, c, partsToBind.at(iter), shed->data());
//             QMultiHash<ObjectId, Group>::iterator iterat =groups.find(c.moduleId);
//             while(iterat!=groups.end() && iterat.key()==c.moduleId){
//                 if(iterat.value().groupList.contains(c)) {
//                    if(!groupFlags[iterat.value().num]){
//                        double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                        double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//                        //���� � ���� ���� ��� ���� �����

//                        if((isThrought) && (loadCur<=mcl)){
//                            iterat.value().groupList.removeOne(c);
//                            //�������� ��������� ������ � ��� ����,

//                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                            myTreeNode temp(partsToBind.at(iter),c);//+1
//                            currentBranch.push_back(temp);

//                            //������� � ������� ������� �����, ������ ���� ���� ������
//                            double a=addLinks(currentBranch, temp, shed->data());
//                            double b=addTime(currentBranch, temp, shed->data());

//                            curDoubleSol+=a;
//                            reversCurSol+=b;

//                            //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
//                            // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
//                            int f= AntipBindingWithGroupsUpdate(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                            //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
//                            currentBranch.pop_back();

//                            // ������ ����� ������� �������

//                            curDoubleSol-=a;
//                            reversCurSol-=b;
//                            //Bounded.pop_back();

//                            iterat.value().groupList.append(c);
//                        }
//                        groupFlags[iterat.value().num]=true;
//                        checked=true;

//                        break;
//                     }
//                     checked=true;
//                 }
//                 iterat++;
//             }

//             if(!checked){
//                 //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
//                 double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                 double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//                 //���� � ���� ���� ��� ���� �����

//                 if((isThrought) && loadCur<=mcl){

//                     //�������� ��������� ������ � ��� ����,

//                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                     myTreeNode temp(partsToBind.at(iter),c);//+1
//                     currentBranch.push_back(temp);

//                     //������� � ������� ������� �����, ������ ���� ���� ������

//                     double a=addLinks(currentBranch, temp, shed->data());
//                     double b=addTime(currentBranch, temp, shed->data());

//                     curDoubleSol+=a;
//                     reversCurSol+=b;

//                     //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
//                     // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
//                     int f= AntipBindingWithGroupsUpdate(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                     //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
//                     currentBranch.pop_back();

//                     // ������ ����� ������� �������
//                     curDoubleSol-=a;
//                     reversCurSol-=b;
//                     //Bounded.pop_back();
//                 }
//            }
//         }

//         Bounded.pop_back();

//         return 1;
//     }
// return 0;
// }

 void recountMinVol(PsevdoGroup& group,Schedule * shed){
     double min=2;
     for(int i =0; i<= group.groupList.size()-1;i++){
         double a =shed->constraints().maxCoreLoad.value(group.groupList.at(i),1.0);
         if( a<min){
             min=a;
         }
     }
 }


// int AntipBindingWithPsevdoGroups(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
//                  const ObjectIdList partsToBind, ObjectIdList Bounded,const QList<CoreId> &cores,Schedule* shed, int iter,
//                            QMultiHash<ObjectId,PsevdoGroup> &groups){
//     QList<myTreeNode> lowerSolution(currentBranch);// ������ ������� ( ���������� � ��� ����� ����������� ��� ���� �������� �������)
//     double upperBound=0;// ����� ������� ������� �������
//     double lowerBound=curDoubleSol;// ����� ������� ������ �������( � ���� ����� ������ ����� �������� �������)
//     if(MaxPosLoad-optDoubleSol>=reversCurSol){
//         bool groupFlags[groups.values().length()];
//         for(int l=0;l<groups.values().length() ;l++){
//             groupFlags[l]=false;
//         }
//         if(iter == partsToBind.length()){//�������� ���� ����� --��������� �����-�� �������, �������� ��������� ��� ������
//             if(curDoubleSol>=optDoubleSol) {
//                 optDoubleSol=curDoubleSol;
//                 optSolution.clear();
//                 foreach (myTreeNode t, currentBranch) {
//                    optSolution.push_back(t);
//                 }
//             }
//             return 1;
//         }
//     //qDebug()<<iter;
//         upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//������� ������� �������-- �������� ���������
//         if(upperBound<=optDoubleSol){
//             return 0;
//         }

//         QMultiMap<ObjectId, CoreId> ep;
//         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//������� ������ �������
//         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//�������� �� �������������
//             optDoubleSol=lowerBound;
//             optSolution.clear();
//             foreach (myTreeNode t, lowerSolution) {
//                optSolution.push_back(t);
//             }
//         }
//         if(optDoubleSol==MaxPosLoad){
//             return 1;
//         }
//      // ������� ������ � �������� iter+1 � ������ ��������������
//         //Bounded.push_back(partsToBind[iter+1]);
//         Bounded.push_back(partsToBind.at(iter));//+1
//         //��� ������� ���������� ���� ��� ���������� �������

//         foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//             bool checked = false;
//             QMultiHash<ObjectId, PsevdoGroup>::iterator iterat =groups.find(c.moduleId);
//             while(iterat!=groups.end() && iterat.key()==c.moduleId){
//                 if(iterat.value().groupList.contains(c)) {
//                    if(!groupFlags[iterat.value().num]){

//                        double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                        double mcl = iterat.value().groupVol;

//                        //���� � ���� ���� ��� ���� �����

//                        if(loadCur<=mcl){
//                            iterat.value().groupList.removeOne(c);
//                            recountMinVol(iterat.value(),shed);
//                            //�������� ��������� ������ � ��� ����,

//                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                            myTreeNode temp(partsToBind.at(iter),c);//+1
//                            currentBranch.push_back(temp);

//                            //������� � ������� ������� �����, ������ ���� ���� ������
//                            double a=addLinks(currentBranch, temp, shed->data());
//                            double b=addTime(currentBranch, temp, shed->data());

//                            curDoubleSol+=a;
//                            reversCurSol+=b;

//                            //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
//                            // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
//                            int f= AntipBindingWithPsevdoGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                            //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
//                            currentBranch.pop_back();

//                            // ������ ����� ������� �������

//                            curDoubleSol-=a;
//                            reversCurSol-=b;
//                            //Bounded.pop_back();

//                            iterat.value().groupList.append(c);
//                            recountMinVol(iterat.value(),shed);
//                        }
//                        groupFlags[iterat.value().num]=true;
//                        checked=true;

//                        break;
//                     }
//                     checked=true;
//                 }
//                 iterat++;
//             }

//             if(!checked){
//                 //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
//                 double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                 double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//                 //���� � ���� ���� ��� ���� �����

//                 if(loadCur<=mcl){

//                     //�������� ��������� ������ � ��� ����,

//                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                     myTreeNode temp(partsToBind.at(iter),c);//+1
//                     currentBranch.push_back(temp);

//                     //������� � ������� ������� �����, ������ ���� ���� ������

//                     double a=addLinks(currentBranch, temp, shed->data());
//                     double b=addTime(currentBranch, temp, shed->data());

//                     curDoubleSol+=a;
//                     reversCurSol+=b;

//                     //�������� �������� � ������� ��������, �����������, ������� ��������, �������������� ��������, ������� ����,
//                     // ����������� ��� ������ � �����������, � ������� ������� ������ ��� ���������������
//                     int f= AntipBindingWithPsevdoGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                     //������ ���� ������ �� ������ �������������� � ������ ���� ������ �� ����� ����
//                     currentBranch.pop_back();

//                     // ������ ����� ������� �������
//                     curDoubleSol-=a;
//                     reversCurSol-=b;
//                     //Bounded.pop_back();
//                 }
//            }
//         }

//         Bounded.pop_back();

//         return 1;
//     }
// return 0;
// }


bool BindAlgoBranchNB::makeBinding(Schedule* schedule)
{
    //qDebug() << "entered";

	// --------- NOT ME START--------//
	if (!schedule || schedule->state() < Schedule::PartitionsReady)
		return false;

	mSchedule = schedule;
    mSchedule->blockSignals(true);

	// some preparations
	mPartsToBind = mSchedule->partitionsAll(); // partitions to bind

	// clearing old binding
	foreach(ObjectId p, mPartsToBind) {
		Partition* part = ProjectData::getPartition(p);

		CoreId chosenOne;
		if (mSchedule->fixedPartitions().contains(part))
			chosenOne = mSchedule->coreForPartition(part->id());
		if (!chosenOne.isValid())
			mSchedule->unbindPartition(part->id());
	}

	// calculating input of fixed binding
    mBound.clear();//
	for(ObjectIdList::iterator it = mPartsToBind.begin(); it != mPartsToBind.end();) {
		CoreId core = mSchedule->coreForPartition(*it);
		if (core.isValid()) {
			mBound.append(*it);
			it = mPartsToBind.erase(it);
		} else ++it;
	}
	// --------- NOT ME END--------//
    QTime era;
    era.start();
	// some needed lists and variables
	//ObjectIdList modules = schedule->config()->modulesIds(); // list of all modules
	QList<CoreId> cores = mSchedule->data()->cores(); // list of all cores
	mOpt.clear(); // clear best solution
	mOptLoad = 0.0; // clear best load solution
	bool opt_flag = false;
	int opt_index = mPartsToBind.size() + 1;

	//some sorts
	qSort(cores);
    ProjectData::sortPartition(mPartsToBind);
    mPartsToBind = gredPartSort(mPartsToBind, mSchedule->data(), opt_flag, opt_index);
    //qDebug() << opt_flag << opt_index << mPartsToBind.size();
    //qDebug() << "размерность (" << mPartsToBind.size() << "," << cores.size() << ")";

    qDebug() << "let the storm begin A New Bound";
   //Antip Start
//    if (!newlog.open(QIODevice::WriteOnly | QIODevice::Text))
//    {
//        qDebug() << "������ ��� �������� �����";
//    }
    if (!oldlog.open(QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "������ ��� �������� �����";
    }
    QList<myTreeNode> CurList; // current solution list
    bool isSuccess = true;
    int MaxDepth = opt_flag && opt_index < mPartsToBind.size() - 1 ? opt_index : mPartsToBind.size(); // maximum depth in tree
    double CurLoadPart = 0.0;
    double revers=0.0;
    int iter=0;

    initPartLoad(mPartsToBind,cores,mSchedule->data());
    initPartPartChannelLoad(mPartsToBind,mSchedule->data());
    foreach(ObjectId ob,mPartsToBind){
        foreach(ObjectId ob2,mPartsToBind){
            //MaxPosLoad+=mSchedule->data()->partPartChannelLoad(ob,ob2);
            MaxPosLoad+=partPartChannelLoad[ob.getId()][ob2.getId()];
            //qDebug()<<ob.toString()<<" - "<<ob2.toString()<<" : "<<mSchedule->data()->partPartChannelLoad(ob,ob2);
        }
    }
    MaxPosLoad/=2;


    //initialize groups of identy cores
   oldBoundStream<<"grouping NEW Bound\n";
    QMultiHash<ObjectId,Group> coreGroups=initCoreGroups(cores,mSchedule, extraConstr);
    int ant= AntipBindingWithGroups(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter, coreGroups, extraConstr, fixedParts, moduleThrConstr);
//    oldBoundStream<<"PSEVDO grouping\n";
//  QMultiHash<ObjectId,PsevdoGroup> coreGroups=initCorePsevdoGroups(cores,mSchedule);
//  int ant= AntipBindingWithPsevdoGroups(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter, coreGroups );
  //int ant= AntipBindingWithGroupsUpdate(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter, coreGroups );


    //int ant= AntipBindingUpperVash(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );
   // int ant= AntipBindingLowerVash(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );
 //int ant= AntipBinding(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );
  //int ant= AntipBindingParts1(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );


    //int ant=AntipBindingCores1(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );
    //int ant=AntipBindingCores2(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter );

    if(mOpt.length()!=mPartsToBind.length()){
        qDebug()<<"Could not make Binding!!!";
        oldBoundStream<<"Could not make Binding!!!\n";
        fail = true;
    }
    qDebug()<<MaxPosLoad-mOptLoad;
    oldBoundStream<<MaxPosLoad-mOptLoad<<"\n";
    //Antip Finish
#ifdef DEBUG
	foreach(myTreeNode t, mOpt) {
        qDebug() << ProjectData::getPartition(t.mPart)->name() <<"()"<< ProjectData::getPartition(t.mPart)->id().toString() << "->" << t.mCore.coreNum << ProjectData::getModule(t.mCore.moduleId)->name();
	}
    qDebug() << "Optimal load" << mOptLoad << "Numbers of vertexes" << numberOfVert;
#endif
	//final assignment
	foreach (myTreeNode tr, mOpt) {
		if (!tr.mCore.isValid()) {
			isSuccess = false;
			continue;
		}
		mSchedule->bindPartition(tr.mPart, tr.mCore);
    }
#ifdef DEBUG
	foreach (CoreId core, mSchedule->data()->cores()) {
		qDebug() << "Core load" << mSchedule->data()->coreLoad(core);
	}
#endif
	isSuccess = (isSuccess && mOpt.size() == MaxDepth) ? true : false;
	mSchedule->blockSignals(false);
	qDebug() << mOpt.size() << ", " << cores.size();
    qDebug("Time elapsed: %d ms", era.elapsed());
//    newBoundStream<< mOpt.size() << ", " << cores.size()<<"\n";
//    newBoundStream<<"Time elapsed: "<<era.elapsed();
//    newlog.close();
    oldBoundStream<< mOpt.size() << ", " << cores.size()<<"\n";
    oldBoundStream<<"Time elapsed: "<<era.elapsed()<<"\n";
    oldBoundStream<<"---------------------------------\n";
    oldlog.close();
	return isSuccess;
}

BindAlgoBranchNB::BindAlgoBranchNB(){}

BindAlgoBranchNB::BindAlgoBranchNB(QMultiMap<ObjectId, CoreId> ec, QMap<ObjectId, CoreId> fixed, Schedule* shed){
    extraConstr = QMultiMap<ObjectId, CoreId>(ec);
    fixedParts = QMap<ObjectId, CoreId> (fixed);
    QMap<ObjectId, double> mt;
    for(int i = 0; i < shed->data()->modules().size(); i++){
        mt.insert((shed->data()->modules().at(i))->id(), MaxThroughput);
    }
    moduleThrConstr = QMap<ObjectId, double>(mt);
}

BindAlgoBranchNB::BindAlgoBranchNB(QMultiMap<ObjectId, CoreId> ec, QMap<ObjectId, CoreId> fixed, QMap<ObjectId, double> thr){
    extraConstr = QMultiMap<ObjectId, CoreId>(ec);
    fixedParts = QMap<ObjectId, CoreId> (fixed);
    moduleThrConstr = QMap<ObjectId, double>(thr);
}

myTreeNode::myTreeNode() {}

myTreeNode::myTreeNode(ObjectId part, CoreId core): mPart(part), mCore(core) {}

myTreeNode::myTreeNode(const myTreeNode &t): mPart(t.mPart), mCore(t.mCore) {}

bool myTreeNode::operator ==(const myTreeNode &t) const
{
	return t.mPart == mPart && mCore == t.mCore;
}

bool myTreeNode::operator !=(const myTreeNode &t) const
{
	return !(t == *this);
}

}


