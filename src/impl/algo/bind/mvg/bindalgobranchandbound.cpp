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
double SchedInt = 10000; //Нужно подсчитать
double CS_END = 1000; //длительность CS - нужно узнать
double MIN_MES_DUR = 0.002;//чему равна длительность передачи сообщения по свободному каналу?
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
        return (this->load)>(other.load);//<- по возрастанию оставшегося места
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
        return (this->connection)>(other.connection);//<- по возрастанию связей
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

//ПРОВЕРИТЬ, ЧТО СУММИРУЕМ - время? длительность? реально загрузку?
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
                          QMap<ObjectId, CoreId> &fixedParts, const QMap<ObjectId, double> &moduleThrConstr,
                          const QMultiMap<ObjectId, QSet<ObjectId>> &notTogether){

     ObjectIdList notBoundedHere;//множество нераспределенных разделов, мы его будем менять в этой функци
     double maxD=0;
     //bool flag=true;//флаг воможности построения решения

     //строим множество нераспределенных разделов
     foreach(ObjectId part, partsToBind){
         if(!Bounded.contains(part))
             notBoundedHere.push_back(part);
     }
     QList<CoreId> availCores;
     ObjectId deleteNow;// раздел, который в данной итерации будет распределен
     CoreId whereToPut;// ядро, на которое раздел выше будет распределен
     // пока множество нераспреденных разделов не пусто и пока можно построить решение
     while(notBoundedHere.length()!=0 ){

         maxD=0;
         //для каждого раздела из множества нераспределенных
         //считаем первые две степени сродства
         foreach( ObjectId part, notBoundedHere){
             availCores.clear();
             double max1=0;
             double max2=0;
             //CoreId maxCor;// ядро соответств первой степени сродства
             int maxCor=0;
             ObjectId maxMod;
             // ядра, на которые можно поместить раздел с учетом их остаточной загрузки

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
                 bool togetherFlag = false;
                 QMultiMap<ObjectId, QSet<ObjectId>>::const_iterator ne = notTogether.find(part);
                 while (ne!=notTogether.end() && ne.key() == part){
                     bool notYetBounded = false;
                     QSet<ObjectId> prohibitedParts = ne.value();
                     QSet<ObjectId>::const_iterator ppIter = ne.value().begin();
                     //check if the parts rom the prohibition already in solution - if no - it is Ok, we can go futher
                     while (ppIter!= ne.value().end()){
                        if (notBoundedHere.contains(*ppIter)){
                            notYetBounded = true;
                            break;
                        }
                        ppIter++;
                     }
                     if(notYetBounded) {
                         ne++;
                         continue;
                      }
                     int prohibitedCount = 0;
                     for (int sol = 0; sol < lowerSolution.size(); sol++){
                         if(lowerSolution.at(sol).mCore == cor && prohibitedParts.contains(lowerSolution.at(sol).mPart)){
                             prohibitedCount++;
                         }
                     }
                     //means that one of the prohibition is violated -> we can not assign this part on this core
                     if (prohibitedCount == prohibitedParts.size() ){
                         togetherFlag = true;
                         break;
                     }
                     ne++;
                 }
                 if(togetherFlag) continue;
                 if (extra) continue;
                 //double loadCur = curLoad(lowerSolution, cor, shed->data()) + shed->data()->partLoad(part,cor);
                 double loadCur = curLoad(lowerSolution, cor, shed->data()) + partLoad[cor.coreNum][part.getId()];
                 bool thr = countEndThroughtput(lowerSolution,cor, part, shed->data(),moduleThrConstr);
                 //double mcl = shed->constraints().maxCoreLoad.value(cor, 1.0);
                 double mcl= shed->constraints().maxCoreLoad[cor];
                 //если в этом ядре еще есть место
                 if(loadCur<=mcl && thr ){
                     availCores.push_back(cor);
                 }
             }

             //если доступных дял раздела ядер нет- нельзя построить решения СТОП
             if(availCores.length()==0){
                 return 0;
             }

             //если доступено только 1 ядро- поместим на него данный раздел, не считая степени сродства
             if(availCores.length()==1){
                 deleteNow=part;
                 whereToPut=availCores[0];
                 break;
             }
             /*bool check;
             if(lowerSolution.count()!=0) check=true;
             else check=false;*/
             //для каждого доступного ядра для данного раздела посчитаем стпени сродства
             int y=0;
             foreach(CoreId cor, availCores){

                 double f=0;
                 bool check= somePartsInModule(lowerSolution,cor);// проверка, есть ли уже в модуле, содержащем данное ядро, хоть 1 раздел
                // if(shed->data()->partLoad(part,cor)!=0){//проверка на деление на 0
                 if(partLoad[cor.coreNum][part.getId()]!=0){
                    if(check){
                        //если в модуле уже есть разделы, считаем степень как связи, добавляемые ядром в модуль деленные на его загрузку ядра
                       // f=addLinks(lowerSolution, myTreeNode(part,cor), shed->data())/shed->data()->partLoad(part,cor);
                        f=addLinks(lowerSolution, myTreeNode(part,cor), shed->data())/partLoad[cor.coreNum][part.getId()];
                    }
                    else{
                        //если модуль пуст- максимальная загрузка ядра, делення на загрузку ядра разделом
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
             //ищем масимальную разницу первых двух стпеней сродства по всем просмотренным разделам
             if(max1-max2>=maxD){
                 maxD=max1-max2;
                 //CoreId ct=shed->data()->mod
                 whereToPut=availCores.at(maxCor);
                 deleteNow=part;
             }
         }
         //убираем из нераспределенных раздел с наибольшей разницей степеней
         notBoundedHere.removeOne(deleteNow);
         //добавляем этот раздел в ядро, соответств макисмальной степени сродства раздела и добавляем это все в текущее решение
         myTreeNode newOne(deleteNow,whereToPut);
         lowerSolution.push_back( newOne);
         // также прибавляем к значению решения связи, даваемые добавленным разделом
         lowerBound+=addLinks(lowerSolution, newOne,shed->data());
     }
     QMap <ObjectId, double> mesDur ;
     QMultiMap<ObjectId, QSet<ObjectId>> parts;
     QMap<ObjectId, double> mesConstr;
     QMap<ObjectId, double> mesMaxDur;
     QMap<ObjectId, double> minChainMessage;
    if (PDCChecker::checkPDC(mesDur, lowerSolution, shed->data(), true, parts, mesConstr, mesMaxDur )){
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
    QList<myTreeNode> curModSolution(ModuleSolution);// изменяемая тут загрузка данного модуля
    QList<ObjectId> availParts;// лист доступных для данного модуля разделов
    CoreId c;
    foreach(CoreId core, shed->data()->cores()){
        if (core.moduleId==mod->id()){
            c=core;
            break;
        }
    }

    double place=ModuleLoad(mod,BoundedInMod,shed);
    double Zn=Zn0;// численное решение
    bool ready=false;//флаг, tckb true- больше ничего нельзя поместить в модуль
    foreach(ObjectId p, partsToBind){//для каждого раздела из вообще всех разделов
        // если раздел еще не содержится в модуле и не запрещен для этого модуля и этот раздел впринципе доступен для данного модуля
        if(!Bounded.contains(p)&&!BoundedInMod.contains(p)&&BannedPartsInMod!=p&&shed->data()->modulesForPartition(p).contains(mod->id())){
            //а также, если в модуле осталось место для этого раздела
           // if(ModuleLoad(mod,BoundedInMod,shed)+shed->data()->partLoad(p,c)<=maxModLoad)
            if(place+partLoad[c.coreNum][p.getId()]<=maxModLoad)
                availParts.push_back(p);// добавим этот раздел в дист доступных данного модуля
        }
    }

    //пока есть доступные разделы и хоть 1 раздел еще помещается на модуль
    while(!ready&& availParts.count()!=0){
        double max=0;//максимум жадного критерия
        ObjectId put;// раздел, соответствующий максимуму критерия
        foreach(ObjectId o, availParts){// для каждого раздела из доступных
            //если в модуле отсалось место для данного раздела
            //if(ModuleLoad(mod,BoundedHere,shed)+shed->data()->partLoad(o,c)<=maxModLoad){
            if(place+partLoad[c.coreNum][o.getId()]<=maxModLoad){
                double d;//значение критерия
                //double w=shed->data()->partLoad(o,c);// загрузка данного модуля данным разделом
                double w=partLoad[c.coreNum][o.getId()];
                if(w!=0){
                    myTreeNode t(o,c);
                    if(BoundedHere.count()==0){//если в данном модуле еще ничего не лежит
                        d=AllLinks(o,availParts,shed)/w;// все связи данного раздела, деленные на его загрузку модуля
                        //d-=shed->data()->partPartChannelLoad(o,BannedPartsInMod);
                        d-=partPartChannelLoad[o.getId()][BannedPartsInMod.getId()];
                    }
                    else{
                        d=addLinks(curModSolution,t,shed->data())/w;// связи раздела с внутренностями модуля, деленные на загрузку
                    }
                    if(d>=max) {// пересчитываем максимум
                        max=d;
                        put=o;
                    }
                }
            }
        }
        if(max!=0){// если максимум не 0
            availParts.removeOne(put);// уберем раздел, соответствующий максимуму из доступных
            Zn+=addLinks(curModSolution,myTreeNode(put,c),shed->data());// добавим связи этого раздела к численному решению
            curModSolution.push_back(myTreeNode(put,c ));// добавим этот раздел в модуль
            BoundedHere.push_back(put);
            place+=partLoad[c.coreNum][put.getId()];

            if(flag) {//если флаг изменения списков правдив
                BoundedInModNew.push_back(put);// добавим раздел в лист распределенных в модуле разделов
                M0M2[put]+=1;// добавим 1 к числу присутствий данного раздела во всех модулях
            }
        }
        else ready=true;// если макс= 0 - все , что можно было добавить мы добавили
    }
    return Zn;
}

double UpperBoundCounting(const QList<myTreeNode> &currentSolution,const ObjectIdList &partsToBind,
                          const ObjectIdList &Bounded, Schedule* shed){
    QMap<ObjectId, double> newCores;// для модулей и их максимальной сквозной загрузки
    QMap<ObjectId,int> M0M2;// для множеств М0 и М2+( для каждого объекта подсчитываем сколько раз он вошел в решения рюкзаков)
    QList<myTreeNode> curModuleSolution[shed->data()->modules().count()];// листы с загрузкой каждого отдельного модуля
    QList<ObjectId> BoundedInMod[shed->data()->modules().count()];//листы с разделами, которые добавлены в каждый модуль в текущем решении
    QList<ObjectId> BoundedInModNew[shed->data()->modules().count()];//листы с разделами, которые добавлены в модуль после первого построения решения
    ObjectId BannedPartsInMod[shed->data()->modules().count()];//запрещенный для модуля раздел
    QMap<Module*,double> Zn0;// MAP с модулем и его решением до алгоритма
    foreach(ObjectId o, partsToBind){
        if(!Bounded.contains(o)){
            M0M2[o]=0;//обнуляем
        }
    }

    int i=0;
    foreach(Module* mod,shed->data()->modules()){// для каждого модуля системы- строим MAP с объемом и побочные листы
        double d=0;
        Zn0[mod]=0;
        foreach(CoreId c,mod->cores()){
         d+=shed->constraints().maxCoreLoad[c];// добавим это ядро в объем модуля
        }
        newCores[mod->id()]=d;
        foreach(myTreeNode temp, currentSolution){//для каждого узла в текущем решении
            if(temp.mCore.moduleId==mod->id()){// если ядро этого узла находится в данном модуле
                BoundedInModNew[i].push_back(temp.mPart);
                BoundedInMod[i].push_back(temp.mPart);// добавим раздел этого узла в лист с номером этого модуля
                Zn0[mod]+= addLinks(curModuleSolution[i],temp , shed->data() );// прибавим связь этого узла
                curModuleSolution[i].push_back(temp);// занесем этот узел в решение данного модуля
            }
        }
        i++;
    }

    bool flag=true; //флаг: надо ли в подсчете решения изменять BoundedInModNew и М0М2; При первом проходе для Zn их изменять надо- мы их там именно строим
    // при построении u1 и u0 их изменять не надо - испортим все.
    QMap<Module*,double> Zn;// MAP с модулем и его решением на первом шаге алгоритма
    double U0=0;
    i=0;
    foreach(Module* mod,shed->data()->modules()){// для каждого модуля считаем его отдельное решение
        Zn[mod]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);
        U0+=Zn[mod];// прибавляем это решение к грубой верхней границе
        i++;
    }
    QList<ObjectId> M0;// множество М0
    QList<ObjectId> M2;// множество М2+
    foreach(ObjectId m, M0M2.keys()){// для каждого объекта этого множества
        if(M0M2[m]==0) M0.push_back(m);// если объект вошел 0 раз в решения- добавим его в М0
        else if(M0M2[m]>1) M2.push_back(m);// елси больше 1 раза- добавим его в М2+
    }
    double maxZn=0;
    foreach(Module* m, shed->data()->modules()){
        if(Zn[m]>maxZn)maxZn=Zn[m];// найдем максимальное Zn
    }

    flag=false;
    double GlobalMaxD=0;// то, что будем вычитать из U0
    double D[M2.count()+M0.count()];// массив для D для каждого из множеств M0 и М2+
    double u1[M0.count()][shed->data()->modules().count()];// u1 для каждого объекта M0 и всех рюкзаков
    int j=0;
    foreach(ObjectId o, M0){// для каждого из М0 считаем u1 и находим минимальную раницу
        i=0;
        double minD=maxZn;
        foreach(Module* mod,shed->data()->modules()){// заставляем включить в каждый модуль текущий раздел из М0
            // проверяем, есть ли вообще место для этого раздела в данном модуле и подходит ли этот модуль для данного
            //if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+shed->data()->partLoad(o,mod->cores()[0])<=newCores[mod->id()]){
            if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+partLoad[mod->cores()[0].coreNum][o.getId()]<=newCores[mod->id()]){
                // пересчитываем побочные листы
                BoundedInMod[i].push_back(o);
                curModuleSolution[i].push_back(myTreeNode(o,mod->cores()[0]));// берем произвольное ядро из модуля
                u1[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// пересчитываем решение
                BoundedInMod[i].pop_back();// возвращаем все назад
                curModuleSolution[i].pop_back();
            }
            else{
             u1[j][i]=0;
            }
            if(Zn[mod]<u1[j][i])u1[j][i]=Zn[mod];
            if(Zn[mod]-u1[j][i]<minD) minD=Zn[mod]-u1[j][i]<minD;// пересчет минимальной разницы
            i++;
        }
        D[j]=minD;// в качестве D для этого раздела берем минимальную разницу
        if(D[j]>GlobalMaxD) GlobalMaxD=D[j];// пересчет максимального D
        j++;
    }

    double u0[M2.count()][shed->data()->modules().count()];// u0 для каждого раздела из М2+ и каждого модуля, куда входит этот раздел

    j=0;

    foreach(ObjectId o, M2){//для каждого раздела из М2+
        double Sum=0;//сичтаем сумму всех разниц
        double maxD=0;// считаем максимальную разницу
        i=0;
        foreach(Module* mod,shed->data()->modules()){//для каждого модуля
            if(BoundedInModNew[i].contains(o)){// если этот раздел включен в модуль
                BannedPartsInMod[i]=o;
                //BannedPartsInMod[i].push_back(o);// запретим этот раздел в этом модуле
                u0[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// пересчитаем решение этого модуля
                //BannedPartsInMod[i].pop_back();// уберем раздел из запрещенных
                if(Zn[mod]<u0[j][i])u0[j][i]=Zn[mod];
                Sum+=Zn[mod]-u0[j][i];// добавим к сумме разность
                if(Zn[mod]-u0[j][i]>maxD) maxD=Zn[mod]-u0[j][i];// пересчитаем максимум разницы
            }
            i++;
        }
        D[M0.count()+j]=Sum-maxD;//посчитаем D для данного раздела
        if(D[M0.count()+j]>GlobalMaxD) GlobalMaxD=D[M0.count()+j];// пересчитаем максимум D
        j++;
    }
    return U0-GlobalMaxD;
}

//int AntipBinding(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
//                 const ObjectIdList &partsToBind, ObjectIdList &Bounded,const QList<CoreId> &cores,Schedule* shed, int iter ){
//    QList<myTreeNode> lowerSolution(currentBranch);// МХФМЪЪ ЦПЮМХЖЮ ( ХГМЮВЮКЭМН Б МЕЕ РЮЙФЕ ДНАЮБКЪЧРЯЪ БЯЕ СГКШ РЕЙСЫЕЦН ПЕЬЕМХЪ)
//    double upperBound=0;// ВХЯКН ПЕЬЕМХЪ БЕПУМЕИ ЦПЮМХЖШ
//    double lowerBound=curDoubleSol;// ВХЯКН ПЕЬЕМХЪ МХФМЕИ ЦПЮМХЖШ( Б МЕЦН РЮЙФЕ БУНДХР ВХЯКН РЕЙСЫЕЦН ПЕЬЕМХЪ)
//    if(MaxPosLoad-optDoubleSol>=reversCurSol){
//        if(iter == partsToBind.length()){//ДНЯРХЦКХ МХГЮ БЕРЙХ --ОНЯРПНХКХ ЙЮЙНЕ-РН ПЕЬЕМХЕ, ОПНБЕПХЛ МЮЯЙНКЭЙН НМН УНПНЬН
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
//        upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//ОНДЯВЕР БЕПУМЕИ ЦПЮМХЖШ-- ОПНБЕПЙЮ НРЯЕВЕМХЪ
//        if(upperBound<=optDoubleSol){
//            return 0;
//        }

//        QMultiMap<ObjectId, CoreId> ep;
//        lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//ОНДЯВЕР МХФМЕИ ЦПЮМХЖШ
//        if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//ОПНБЕПЙЮ МЮ НОРХЛЮКЭМНЯРЭ
//            optDoubleSol=lowerBound;
//            optSolution.clear();
//            foreach (myTreeNode t, lowerSolution) {
//               optSolution.push_back(t);
//            }
//        }
//        if(optDoubleSol==MaxPosLoad){return 1;}
//     // ДНАЮБХЛ ПЮГДЕК Я ХМДЕЙЯНЛ iter+1 Б ЯОХЯНЙ ПЮЯОПЕДЕКЕММШУ
//        //Bounded.push_back(partsToBind[iter+1]);
//        Bounded.push_back(partsToBind.at(iter));//+1
//        //ДКЪ ЙЮФДНЦН ДНЯРСОМНЦН ЪДПЮ ДКЪ ЯКЕДСЧЫЕЦН ПЮГДЕКЮ

//        foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//            //double loadCur = curLoad(currentBranch, c,  shed->data()) + shed->data()->partLoad(partsToBind[iter],c);//+1
//            double loadCur = curLoad(currentBranch, c,  shed->data()) + partLoad[c.coreNum][partsToBind[iter].getId()];
//            double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//            //ЕЯКХ Б ЩРНЛ ЪДПЕ ЕЫЕ ЕЯРЭ ЛЕЯРН

//            if(loadCur<=mcl){

//                //МЮГМЮВХЛ ЯКЕДСЧЫХИ ПЮГДЕК Б ЩРН ЪДПН,

//               // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                myTreeNode temp(partsToBind.at(iter),c);//+1
//                currentBranch.push_back(temp);

//                //ДНАЮБХЛ Б РЕЙСЫЕЕ ПЕЬЕМХЕ ЯБЪГХ, ЙНРПШЕ ДЮЕР ЩРНР ПЮГДЕК
//                double a=addLinks(currentBranch, temp, shed->data());
//                double b=addTime(currentBranch, temp, shed->data());

//                curDoubleSol+=a;
//                reversCurSol+=b;

//                //ГЮОСЯРХЛ ПЕЙСПЯХЧ Я РЕЙСЫХЛ ПЕЬЕМХЕЛ, НОРХЛЮКЭМШЛ, МЮАНПНЛ ПЮГДЕКНБ, ПЮЯОПЕДЕКЕММШУ ПЮГДЕКНБ, МЮАНПНЛ ЪДЕП,
//                // ПЮЯОХЯЮМХЕЛ ДКЪ ДЮММШУ Х НЦПЮМХВЕМХИ, Х МНЛЕПНЛ ПЮГДЕКЮ РНКЭЙН ВРН ПЮЯОПЕДЕКЕММНЦН
//                int f= AntipBinding(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1);

//                //САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЯОХЯЙЮ ПЮЯОПЕДЕКЕММШУ Х САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЩРНЦН ЪДПЮ
//                currentBranch.pop_back();

//                // САЕПЕЛ ЯБЪГЭ ДЮММНЦН ПЮГДЕКЮ
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
//* возвращает минимум из двух вариантов верхних границ
//* либо U0-maxDi
//* либо U0-sumDi/2
//****
double UpperBoundCountingUpdate(const QList<myTreeNode> &currentSolution,const ObjectIdList &partsToBind,
                                const ObjectIdList &Bounded, Schedule* shed){
    QMap<ObjectId, double> newCores;// для модулей и их максимальной сквозной загрузки
    QMap<ObjectId,int> M0M2;// для множеств М0 и М2+( для каждого объекта подсчитываем сколько раз он вошел в решения рюкзаков)
    QList<myTreeNode> curModuleSolution[shed->data()->modules().count()];// листы с загрузкой каждого отдельного модуля
    QList<ObjectId> BoundedInMod[shed->data()->modules().count()];//листы с разделами, которые добавлены в каждый модуль в текущем решении
    QList<ObjectId> BoundedInModNew[shed->data()->modules().count()];//листы с разделами, которые добавлены в модуль после первого построения решения
    ObjectId BannedPartsInMod[shed->data()->modules().count()];//запрещенный для модуля раздел
    QMap<Module*,double> Zn0;// MAP с модулем и его решением до алгоритма
    foreach(ObjectId o, partsToBind){
        if(!Bounded.contains(o)){
            M0M2[o]=0;//обнуляем
        }
    }

    int i=0;
    foreach(Module* mod,shed->data()->modules()){// для каждого модуля системы- строим MAP с объемом и побочные листы
        double d=0;
        Zn0[mod]=0;
        foreach(CoreId c,mod->cores()){
            d+=shed->constraints().maxCoreLoad[c];// добавим это ядро в объем модуля
        }
        newCores[mod->id()]=d;
        foreach(myTreeNode temp, currentSolution){//для каждого узла в текущем решении
            if(temp.mCore.moduleId==mod->id()){// если ядро этого узла находится в данном модуле
                BoundedInModNew[i].push_back(temp.mPart);
                BoundedInMod[i].push_back(temp.mPart);// добавим раздел этого узла в лист с номером этого модуля
                Zn0[mod]+= addLinks(curModuleSolution[i],temp , shed->data() );// прибавим связь этого узла
                curModuleSolution[i].push_back(temp);// занесем этот узел в решение данного модуля
            }
        }
        i++;
    }

    bool flag=true; //флаг: надо ли в подсчете решения изменять BoundedInModNew и М0М2; При первом проходе для Zn их изменять надо- мы их там именно строим
    // при построении u1 и u0 их изменять не надо - испортим все.
    QMap<Module*,double> Zn;// MAP с модулем и его решением на первом шаге алгоритма
    double U0=0;
    i=0;
    foreach(Module* mod,shed->data()->modules()){// для каждого модуля считаем его отдельное решение
        Zn[mod]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);
        U0+=Zn[mod];// прибавляем это решение к грубой верхней границе
        i++;
    }
    QList<ObjectId> M0;// множество М0
    QList<ObjectId> M2;// множество М2+
    foreach(ObjectId m, M0M2.keys()){// для каждого объекта этого множества
        if(M0M2[m]==0) M0.push_back(m);// если объект вошел 0 раз в решения- добавим его в М0
        else if(M0M2[m]>1) M2.push_back(m);// елси больше 1 раза- добавим его в М2+
    }
    double maxZn=0;
    foreach(Module* m, shed->data()->modules()){
        if(Zn[m]>maxZn)maxZn=Zn[m];// найдем максимальное Zn
    }

    flag=false;
    double GlobalMaxD=0;// то, что будем вычитать из U0
    double SumD=0;
    double D[M2.count()+M0.count()];// массив для D для каждого из множеств M0 и М2+
    double u1[M0.count()][shed->data()->modules().count()];// u1 для каждого объекта M0 и всех рюкзаков
    int j=0;
    foreach(ObjectId o, M0){// для каждого из М0 считаем u1 и находим минимальную раницу
        i=0;
        double minD=maxZn;
        foreach(Module* mod,shed->data()->modules()){// заставляем включить в каждый модуль текущий раздел из М0
            // проверяем, есть ли вообще место для этого раздела в данном модуле и подходит ли этот модуль для данного
            //if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+shed->data()->partLoad(o,mod->cores()[0])<=newCores[mod->id()]){
            if(shed->data()->modulesForPartition(o).contains(mod->id()) && ModuleLoad(mod,BoundedInMod[i],shed)+partLoad[mod->cores()[0].coreNum][o.getId()]<=newCores[mod->id()]){
                // пересчитываем побочные листы
                BoundedInMod[i].push_back(o);
                curModuleSolution[i].push_back(myTreeNode(o,mod->cores()[0]));// берем произвольное ядро из модуля
                u1[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// пересчитываем решение
                BoundedInMod[i].pop_back();// возвращаем все назад
                curModuleSolution[i].pop_back();
            }
            else{
                u1[j][i]=0;
            }
            if(Zn[mod]<u1[j][i])u1[j][i]=Zn[mod];
            if(Zn[mod]-u1[j][i]<minD) minD=Zn[mod]-u1[j][i]<minD;// пересчет минимальной разницы
            i++;
        }
        D[j]=minD;// в качестве D для этого раздела берем минимальную разницу
        SumD+=D[j];
        if(D[j]>GlobalMaxD) GlobalMaxD=D[j];// пересчет максимального D
        j++;
    }

    double u0[M2.count()][shed->data()->modules().count()];// u0 для каждого раздела из М2+ и каждого модуля, куда входит этот раздел

    j=0;

    foreach(ObjectId o, M2){//для каждого раздела из М2+
        double Sum=0;//сичтаем сумму всех разниц
        double maxD=0;// считаем максимальную разницу
        i=0;
        foreach(Module* mod,shed->data()->modules()){//для каждого модуля
            if(BoundedInModNew[i].contains(o)){// если этот раздел включен в модуль
                BannedPartsInMod[i]=o;
                //BannedPartsInMod[i].push_back(o);// запретим этот раздел в этом модуле
                u0[j][i]=CountBound(mod,newCores[mod->id()],curModuleSolution[i],Bounded,BannedPartsInMod[i],BoundedInMod[i],BoundedInModNew[i],M0M2,shed,flag,partsToBind,Zn0[mod]);// пересчитаем решение этого модуля
                //BannedPartsInMod[i].pop_back();// уберем раздел из запрещенных
                if(Zn[mod]<u0[j][i])u0[j][i]=Zn[mod];
                Sum+=Zn[mod]-u0[j][i];// добавим к сумме разность
                if(Zn[mod]-u0[j][i]>maxD) maxD=Zn[mod]-u0[j][i];// пересчитаем максимум разницы
            }
            i++;
        }
        D[M0.count()+j]=Sum-maxD;//посчитаем D для данного раздела
        SumD+=D[M0.count()+j];
        if(D[M0.count()+j]>GlobalMaxD) GlobalMaxD=D[M0.count()+j];// пересчитаем максимум D
        j++;
    }
    double SumD2=SumD/2;
    if(GlobalMaxD>SumD2) return U0-GlobalMaxD;
    else{ return U0-SumD2;}
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
                                //создать группу и добавить оба объекта
                                Group grStruct;
                                grStruct.num=num;
                                num++;
                                grStruct.groupList.append(cores.at(i));
                                grStruct.groupList.append(cores.at(j));
                                groups.insert(cores.at(i).moduleId,grStruct);
                                groupEx=true;
                            }
                            else{
                                //добавить только внутренний объект
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
                //создать группу и добавить объект
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
                            QMap<ObjectId, CoreId> &fixedParts, const QMap<ObjectId, double> &moduleThrConstr,
                            const QMultiMap<ObjectId, QSet<ObjectId>> &notTogether){
     QList<myTreeNode> lowerSolution(currentBranch);//нижняя граница ( изначально в нее также добавляются все узлы текущего решения)
     double upperBound=0;// число решения верхней границы
     double lowerBound=curDoubleSol;// число решения нижней границы( в него также входит число текущего решения)
     if(MaxPosLoad-optDoubleSol>=reversCurSol){
         bool groupFlags[groups.values().length()];
         for(int l=0;l<groups.values().length() ;l++){
             groupFlags[l]=false;
         }
         if(iter == partsToBind.length()){//достигли низа ветки --построили какое-то решение, проверим насколько оно хорошо
             QMap <ObjectId, double> mesDur ;
             QMultiMap<ObjectId, QSet<ObjectId>> parts;
             QMap<ObjectId, double> mesConstr;
             QMap<ObjectId, double> mesMaxDur;
             QMap<ObjectId, double> minChainMessage;
             if((curDoubleSol>=optDoubleSol) && (PDCChecker::checkPDC(mesDur, currentBranch, shed->data(), true, parts, mesConstr, mesMaxDur))) {
                 optDoubleSol=curDoubleSol;
                 optSolution.clear();
                 foreach (myTreeNode t, currentBranch) {
                    optSolution.push_back(t);
                 }
             }
             return 1;
         }
     //qDebug()<<iter;
         upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//подсчет верхней границы-- проверка отсечения
         if(upperBound<=optDoubleSol && optDoubleSol>0.0){
             return 0;
         }
         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, extraConstr, fixedParts, moduleThrConstr, notTogether);//подсчет нижней границы
         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//проверка на оптимальность
             optDoubleSol=lowerBound;
             optSolution.clear();
             foreach (myTreeNode t, lowerSolution) {
                optSolution.push_back(t);
             }
         }
         if(optDoubleSol==MaxPosLoad){
             return 1;
         }
      // добавим раздел с индексом iter+1 в список распределенных
         //Bounded.push_back(partsToBind[iter+1]);
         Bounded.push_back(partsToBind.at(iter));//+1
         //для каждого доступного ядра для следующего раздела

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
             bool togetherFlag = false;
             QMultiMap<ObjectId, QSet<ObjectId>>::const_iterator ne = notTogether.find(partsToBind[iter]);
             while (ne!=notTogether.end() && ne.key() == partsToBind[iter]){
                 QSet<ObjectId> prohibitedParts = ne.value();
                 int prohibitedCount = 0;
                 for (int sol = 0; sol < currentBranch.size(); sol++){
                     if(currentBranch.at(sol).mCore == c && prohibitedParts.contains(currentBranch.at(sol).mPart)){
                         prohibitedCount++;
                     }
                 }
                 //means that one of the prohibition is violated -> we can not assign this part on this core
                 if (prohibitedCount == prohibitedParts.size() ){
                     togetherFlag = true;
                     break;
                 }
                 ne++;
             }
             if(togetherFlag) continue;
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

                        //если в этом ядре еще есть место

                        if((isThrought) && (loadCur<=mcl)){
                            iterat.value().groupList.removeOne(c);
                            //назначим следующий раздел в это ядро,

                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
                            myTreeNode temp(partsToBind.at(iter),c);//+1
                            currentBranch.push_back(temp);

                            //добавим в текущее решение связи, котрые дает этот раздел
                            double a=addLinks(currentBranch, temp, shed->data());
                            double b=addTime(currentBranch, temp, shed->data());

                            curDoubleSol+=a;
                            reversCurSol+=b;


                            //запустим рекурсию с текущим решением, оптимальным, набором разделов, распределенных разделов, набором ядер,
                            // расписанием для данных и ограничений, и номером раздела только что распределенного
                            int f= AntipBindingWithGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups, extraConstr, fixedParts,moduleThrConstr, notTogether);

                            //уберем этот раздел из списка распределенных и уберем этот раздел из этого ядра
                            currentBranch.pop_back();

                            // уберем связь данного раздела

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

                 //если в этом ядре еще есть место

                 if((isThrought) && (loadCur<=mcl)){

                     //назначим следующий раздел в это ядро,

                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
                     myTreeNode temp(partsToBind.at(iter),c);//+1
                     currentBranch.push_back(temp);

                     //добавим в текущее решение связи, котрые дает этот раздел

                     double a=addLinks(currentBranch, temp, shed->data());
                     double b=addTime(currentBranch, temp, shed->data());

                     curDoubleSol+=a;
                     reversCurSol+=b;

                     //запустим рекурсию с текущим решением, оптимальным, набором разделов, распределенных разделов, набором ядер,
                     // расписанием для данных и ограничений, и номером раздела только что распределенного
                     int f= AntipBindingWithGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups, extraConstr, fixedParts, moduleThrConstr, notTogether);

                     //уберем этот раздел из списка распределенных и уберем этот раздел из этого ядра
                     currentBranch.pop_back();

                     // уберем связь данного раздела
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
//* ОНДЯВЕР БЕПУМЕИ ЦПЮМХЖШ ВЕПЕГ ЛХМХЛСЛ
//* ЛНФМН ПЮЯЙНЛЛЕМРХПНБЮРЭ Х ОЮПЮККЕКЭМН БЕЯРХ ОНДЯВЕР НАШВМНИ ЦПЮМХЖШ
//* БНГЛНФМН ОНЛНФЕР НРЯКЕДХРЭ МЕБЕПМШЕ НРЯЕВЕМХЪ
//* МН ЯМЮВЮКН МЮДН ХЯЯКЕДНБЮРЭ ОПНЯРН МЮ ЩТТЕЙРХБМНЯРЭ
//****
// int AntipBindingWithGroupsUpdate(QList<myTreeNode> &currentBranch, QList<myTreeNode> &optSolution, double reversCurSol,double curDoubleSol, double &optDoubleSol,
//                  const ObjectIdList partsToBind, ObjectIdList Bounded,const QList<CoreId> &cores,Schedule* shed, int iter,
//                            QMultiHash<ObjectId,Group> &groups){
//     QList<myTreeNode> lowerSolution(currentBranch);// МХФМЪЪ ЦПЮМХЖЮ ( ХГМЮВЮКЭМН Б МЕЕ РЮЙФЕ ДНАЮБКЪЧРЯЪ БЯЕ СГКШ РЕЙСЫЕЦН ПЕЬЕМХЪ)
//     double upperBoundUpdate=0;// ВХЯКН ПЕЬЕМХЪ БЕПУМЕИ ЦПЮМХЖШ
//     //double upperBound=0;
//     double lowerBound=curDoubleSol;// ВХЯКН ПЕЬЕМХЪ МХФМЕИ ЦПЮМХЖШ( Б МЕЦН РЮЙФЕ БУНДХР ВХЯКН РЕЙСЫЕЦН ПЕЬЕМХЪ)
//     if(MaxPosLoad-optDoubleSol>=reversCurSol){
//         bool groupFlags[groups.values().length()];
//         for(int l=0;l<groups.values().length() ;l++){
//             groupFlags[l]=false;
//         }
//         if(iter == partsToBind.length()){//ДНЯРХЦКХ МХГЮ БЕРЙХ --ОНЯРПНХКХ ЙЮЙНЕ-РН ПЕЬЕМХЕ, ОПНБЕПХЛ МЮЯЙНКЭЙН НМН УНПНЬН
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
//         upperBoundUpdate=UpperBoundCountingUpdate(currentBranch,partsToBind,Bounded, shed);//ОНДЯВЕР БЕПУМЕИ ЦПЮМХЖШ-- ОПНБЕПЙЮ НРЯЕВЕМХЪ
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
//         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//ОНДЯВЕР МХФМЕИ ЦПЮМХЖШ
//         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//ОПНБЕПЙЮ МЮ НОРХЛЮКЭМНЯРЭ
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
//      // ДНАЮБХЛ ПЮГДЕК Я ХМДЕЙЯНЛ iter+1 Б ЯОХЯНЙ ПЮЯОПЕДЕКЕММШУ
//         //Bounded.push_back(partsToBind[iter+1]);
//         Bounded.push_back(partsToBind.at(iter));//+1
//         //ДКЪ ЙЮФДНЦН ДНЯРСОМНЦН ЪДПЮ ДКЪ ЯКЕДСЧЫЕЦН ПЮГДЕКЮ

//         foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//             bool checked = false;
//             bool isThrought = countEndThroughtput(currentBranch, c, partsToBind.at(iter), shed->data());
//             QMultiHash<ObjectId, Group>::iterator iterat =groups.find(c.moduleId);
//             while(iterat!=groups.end() && iterat.key()==c.moduleId){
//                 if(iterat.value().groupList.contains(c)) {
//                    if(!groupFlags[iterat.value().num]){
//                        double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                        double mcl = shed->constraints().maxCoreLoad.value(c, 1.0);

//                        //ЕЯКХ Б ЩРНЛ ЪДПЕ ЕЫЕ ЕЯРЭ ЛЕЯРН

//                        if((isThrought) && (loadCur<=mcl)){
//                            iterat.value().groupList.removeOne(c);
//                            //МЮГМЮВХЛ ЯКЕДСЧЫХИ ПЮГДЕК Б ЩРН ЪДПН,

//                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                            myTreeNode temp(partsToBind.at(iter),c);//+1
//                            currentBranch.push_back(temp);

//                            //ДНАЮБХЛ Б РЕЙСЫЕЕ ПЕЬЕМХЕ ЯБЪГХ, ЙНРПШЕ ДЮЕР ЩРНР ПЮГДЕК
//                            double a=addLinks(currentBranch, temp, shed->data());
//                            double b=addTime(currentBranch, temp, shed->data());

//                            curDoubleSol+=a;
//                            reversCurSol+=b;

//                            //ГЮОСЯРХЛ ПЕЙСПЯХЧ Я РЕЙСЫХЛ ПЕЬЕМХЕЛ, НОРХЛЮКЭМШЛ, МЮАНПНЛ ПЮГДЕКНБ, ПЮЯОПЕДЕКЕММШУ ПЮГДЕКНБ, МЮАНПНЛ ЪДЕП,
//                            // ПЮЯОХЯЮМХЕЛ ДКЪ ДЮММШУ Х НЦПЮМХВЕМХИ, Х МНЛЕПНЛ ПЮГДЕКЮ РНКЭЙН ВРН ПЮЯОПЕДЕКЕММНЦН
//                            int f= AntipBindingWithGroupsUpdate(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                            //САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЯОХЯЙЮ ПЮЯОПЕДЕКЕММШУ Х САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЩРНЦН ЪДПЮ
//                            currentBranch.pop_back();

//                            // САЕПЕЛ ЯБЪГЭ ДЮММНЦН ПЮГДЕКЮ

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

//                 //ЕЯКХ Б ЩРНЛ ЪДПЕ ЕЫЕ ЕЯРЭ ЛЕЯРН

//                 if((isThrought) && loadCur<=mcl){

//                     //МЮГМЮВХЛ ЯКЕДСЧЫХИ ПЮГДЕК Б ЩРН ЪДПН,

//                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                     myTreeNode temp(partsToBind.at(iter),c);//+1
//                     currentBranch.push_back(temp);

//                     //ДНАЮБХЛ Б РЕЙСЫЕЕ ПЕЬЕМХЕ ЯБЪГХ, ЙНРПШЕ ДЮЕР ЩРНР ПЮГДЕК

//                     double a=addLinks(currentBranch, temp, shed->data());
//                     double b=addTime(currentBranch, temp, shed->data());

//                     curDoubleSol+=a;
//                     reversCurSol+=b;

//                     //ГЮОСЯРХЛ ПЕЙСПЯХЧ Я РЕЙСЫХЛ ПЕЬЕМХЕЛ, НОРХЛЮКЭМШЛ, МЮАНПНЛ ПЮГДЕКНБ, ПЮЯОПЕДЕКЕММШУ ПЮГДЕКНБ, МЮАНПНЛ ЪДЕП,
//                     // ПЮЯОХЯЮМХЕЛ ДКЪ ДЮММШУ Х НЦПЮМХВЕМХИ, Х МНЛЕПНЛ ПЮГДЕКЮ РНКЭЙН ВРН ПЮЯОПЕДЕКЕММНЦН
//                     int f= AntipBindingWithGroupsUpdate(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                     //САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЯОХЯЙЮ ПЮЯОПЕДЕКЕММШУ Х САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЩРНЦН ЪДПЮ
//                     currentBranch.pop_back();

//                     // САЕПЕЛ ЯБЪГЭ ДЮММНЦН ПЮГДЕКЮ
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
//     QList<myTreeNode> lowerSolution(currentBranch);// МХФМЪЪ ЦПЮМХЖЮ ( ХГМЮВЮКЭМН Б МЕЕ РЮЙФЕ ДНАЮБКЪЧРЯЪ БЯЕ СГКШ РЕЙСЫЕЦН ПЕЬЕМХЪ)
//     double upperBound=0;// ВХЯКН ПЕЬЕМХЪ БЕПУМЕИ ЦПЮМХЖШ
//     double lowerBound=curDoubleSol;// ВХЯКН ПЕЬЕМХЪ МХФМЕИ ЦПЮМХЖШ( Б МЕЦН РЮЙФЕ БУНДХР ВХЯКН РЕЙСЫЕЦН ПЕЬЕМХЪ)
//     if(MaxPosLoad-optDoubleSol>=reversCurSol){
//         bool groupFlags[groups.values().length()];
//         for(int l=0;l<groups.values().length() ;l++){
//             groupFlags[l]=false;
//         }
//         if(iter == partsToBind.length()){//ДНЯРХЦКХ МХГЮ БЕРЙХ --ОНЯРПНХКХ ЙЮЙНЕ-РН ПЕЬЕМХЕ, ОПНБЕПХЛ МЮЯЙНКЭЙН НМН УНПНЬН
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
//         upperBound=UpperBoundCounting(currentBranch,partsToBind,Bounded, shed);//ОНДЯВЕР БЕПУМЕИ ЦПЮМХЖШ-- ОПНБЕПЙЮ НРЯЕВЕМХЪ
//         if(upperBound<=optDoubleSol){
//             return 0;
//         }

//         QMultiMap<ObjectId, CoreId> ep;
//         lowerBound=lowerBoundCounting(lowerSolution,lowerBound,partsToBind,Bounded, shed, ep);//ОНДЯВЕР МХФМЕИ ЦПЮМХЖШ
//         if(lowerBound>optDoubleSol&& lowerSolution.count()==partsToBind.length()) {//ОПНБЕПЙЮ МЮ НОРХЛЮКЭМНЯРЭ
//             optDoubleSol=lowerBound;
//             optSolution.clear();
//             foreach (myTreeNode t, lowerSolution) {
//                optSolution.push_back(t);
//             }
//         }
//         if(optDoubleSol==MaxPosLoad){
//             return 1;
//         }
//      // ДНАЮБХЛ ПЮГДЕК Я ХМДЕЙЯНЛ iter+1 Б ЯОХЯНЙ ПЮЯОПЕДЕКЕММШУ
//         //Bounded.push_back(partsToBind[iter+1]);
//         Bounded.push_back(partsToBind.at(iter));//+1
//         //ДКЪ ЙЮФДНЦН ДНЯРСОМНЦН ЪДПЮ ДКЪ ЯКЕДСЧЫЕЦН ПЮГДЕКЮ

//         foreach(CoreId c, shed->data()->coresForPartition(partsToBind[iter])){//+1
//             bool checked = false;
//             QMultiHash<ObjectId, PsevdoGroup>::iterator iterat =groups.find(c.moduleId);
//             while(iterat!=groups.end() && iterat.key()==c.moduleId){
//                 if(iterat.value().groupList.contains(c)) {
//                    if(!groupFlags[iterat.value().num]){

//                        double loadCur = curLoad(currentBranch, c, shed->data())+ partLoad[c.coreNum][partsToBind[iter].getId()];
//                        double mcl = iterat.value().groupVol;

//                        //ЕЯКХ Б ЩРНЛ ЪДПЕ ЕЫЕ ЕЯРЭ ЛЕЯРН

//                        if(loadCur<=mcl){
//                            iterat.value().groupList.removeOne(c);
//                            recountMinVol(iterat.value(),shed);
//                            //МЮГМЮВХЛ ЯКЕДСЧЫХИ ПЮГДЕК Б ЩРН ЪДПН,

//                           // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                            myTreeNode temp(partsToBind.at(iter),c);//+1
//                            currentBranch.push_back(temp);

//                            //ДНАЮБХЛ Б РЕЙСЫЕЕ ПЕЬЕМХЕ ЯБЪГХ, ЙНРПШЕ ДЮЕР ЩРНР ПЮГДЕК
//                            double a=addLinks(currentBranch, temp, shed->data());
//                            double b=addTime(currentBranch, temp, shed->data());

//                            curDoubleSol+=a;
//                            reversCurSol+=b;

//                            //ГЮОСЯРХЛ ПЕЙСПЯХЧ Я РЕЙСЫХЛ ПЕЬЕМХЕЛ, НОРХЛЮКЭМШЛ, МЮАНПНЛ ПЮГДЕКНБ, ПЮЯОПЕДЕКЕММШУ ПЮГДЕКНБ, МЮАНПНЛ ЪДЕП,
//                            // ПЮЯОХЯЮМХЕЛ ДКЪ ДЮММШУ Х НЦПЮМХВЕМХИ, Х МНЛЕПНЛ ПЮГДЕКЮ РНКЭЙН ВРН ПЮЯОПЕДЕКЕММНЦН
//                            int f= AntipBindingWithPsevdoGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                            //САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЯОХЯЙЮ ПЮЯОПЕДЕКЕММШУ Х САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЩРНЦН ЪДПЮ
//                            currentBranch.pop_back();

//                            // САЕПЕЛ ЯБЪГЭ ДЮММНЦН ПЮГДЕКЮ

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

//                 //ЕЯКХ Б ЩРНЛ ЪДПЕ ЕЫЕ ЕЯРЭ ЛЕЯРН

//                 if(loadCur<=mcl){

//                     //МЮГМЮВХЛ ЯКЕДСЧЫХИ ПЮГДЕК Б ЩРН ЪДПН,

//                    // Bounded.push_back(partsToBind[iter+1].ObjectId);
//                     myTreeNode temp(partsToBind.at(iter),c);//+1
//                     currentBranch.push_back(temp);

//                     //ДНАЮБХЛ Б РЕЙСЫЕЕ ПЕЬЕМХЕ ЯБЪГХ, ЙНРПШЕ ДЮЕР ЩРНР ПЮГДЕК

//                     double a=addLinks(currentBranch, temp, shed->data());
//                     double b=addTime(currentBranch, temp, shed->data());

//                     curDoubleSol+=a;
//                     reversCurSol+=b;

//                     //ГЮОСЯРХЛ ПЕЙСПЯХЧ Я РЕЙСЫХЛ ПЕЬЕМХЕЛ, НОРХЛЮКЭМШЛ, МЮАНПНЛ ПЮГДЕКНБ, ПЮЯОПЕДЕКЕММШУ ПЮГДЕКНБ, МЮАНПНЛ ЪДЕП,
//                     // ПЮЯОХЯЮМХЕЛ ДКЪ ДЮММШУ Х НЦПЮМХВЕМХИ, Х МНЛЕПНЛ ПЮГДЕКЮ РНКЭЙН ВРН ПЮЯОПЕДЕКЕММНЦН
//                     int f= AntipBindingWithPsevdoGroups(currentBranch,optSolution,reversCurSol,curDoubleSol,optDoubleSol, partsToBind,Bounded,cores,shed,iter+1,groups);

//                     //САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЯОХЯЙЮ ПЮЯОПЕДЕКЕММШУ Х САЕПЕЛ ЩРНР ПЮГДЕК ХГ ЩРНЦН ЪДПЮ
//                     currentBranch.pop_back();

//                     // САЕПЕЛ ЯБЪГЭ ДЮММНЦН ПЮГДЕКЮ
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
    //qDebug() << "я─п╟пЇп╪п╣я─пҐп╬я│я┌я▄ (" << mPartsToBind.size() << "," << cores.size() << ")";

    qDebug() << "let the storm begin A New Bound";
   //Antip Start
//    if (!newlog.open(QIODevice::WriteOnly | QIODevice::Text))
//    {
//        qDebug() << "нЬХАЙЮ ОПХ НРЙПШРХХ ТЮИКЮ";
//    }
    if (!oldlog.open(QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "Ошибка при открытии файла";
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
    int ant= AntipBindingWithGroups(CurList, mOpt,revers, CurLoadPart,mOptLoad,mPartsToBind, mBound,cores,mSchedule,iter, coreGroups, extraConstr, fixedParts, moduleThrConstr,notTogether);
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

BindAlgoBranchNB::BindAlgoBranchNB(QMultiMap<ObjectId, CoreId> ec, QMultiMap<ObjectId, QSet<ObjectId>> nT, QMap<ObjectId, CoreId> fixed, Schedule* shed){
    extraConstr = QMultiMap<ObjectId, CoreId>(ec);
    fixedParts = QMap<ObjectId, CoreId> (fixed);
    notTogether = QMultiMap<ObjectId, QSet<ObjectId>> (nT);
    QMap<ObjectId, double> mt;
    for(int i = 0; i < shed->data()->modules().size(); i++){
        mt.insert((shed->data()->modules().at(i))->id(), MaxThroughput);
    }
    moduleThrConstr = QMap<ObjectId, double>(mt);
}

BindAlgoBranchNB::BindAlgoBranchNB(QMultiMap<ObjectId, CoreId> ec, QMultiMap<ObjectId, QSet<ObjectId>> nT, QMap<ObjectId, CoreId> fixed, QMap<ObjectId, double> thr){
    extraConstr = QMultiMap<ObjectId, CoreId>(ec);
    fixedParts = QMap<ObjectId, CoreId> (fixed);
    moduleThrConstr = QMap<ObjectId, double>(thr);
    notTogether = QMultiMap<ObjectId, QSet<ObjectId>> (nT);
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


