#include "pdcchecker.h"

#include <iostream>
#include <string>
#include "math.h"

#include <QDebug>
#include <QTime>
#include <QVector>
#include <QFile>

#include "scheduledata.h"
#include "projectdata.h"
#include "sdicounting.h"

namespace A653 {

QFile loggerSDI("SDI_log.txt");
QTextStream sdiStream(&loggerSDI);

QMap<ObjectId, CoreId> PDCChecker::initTaskCore (const QList<myTreeNode> &myList, const  QList<Task*> tasks){
    QMap<ObjectId, CoreId> taskCore;
    for (int i = 0; i < tasks.length(); i++){
        for (int j = 0; j < myList.size(); j++){
            if(myList.at(j).mPart == tasks.at(i)->partitionId()){
               taskCore.insert(tasks.at(i)->id(), myList.at(j).mCore);
            }
        }
    }
    return taskCore;
}

void initMesageDur(ScheduleData *data, const QMap<ObjectId, CoreId> &taskCore, QMap <ObjectId, double> &dur ){

    for (int i = 0; i < data->messages().size(); i++){
        Message* m = data->messages().at(i);
        if (m->receiver()->partitionId()== m->sender()->partitionId() || taskCore.find(m->receiver()->id()).value().moduleId==taskCore.find(m->sender()->id()).value().moduleId){
            dur.insert(m->id(), (double) m->DefaultMDur);
        }
        else dur.insert(m->id(), (double) m->DefaultCDur);
    }
}

QList<ObjectId> findTaskBySDI(const QMap <ObjectId, CoreId> &taskCore, const QMap <ObjectId, QList<double>> &SDI, const CoreId c, double d){
    QList<ObjectId> temp;
    foreach (ObjectId t, taskCore.keys()){
        if(taskCore.find(t).value()!=c){
            continue;
        }
        for (int i = 0; i < SDI.find(t).value().size(); i++){
            if(abs(SDI.find(t).value().at(i)-d) <= 0.00001){
                temp.append(t);
            }
        }
    }

    return temp;
}

double countNearestLeft(CoreId c, double left, const QMap <ObjectId, CoreId> &taskCore, const QMap <ObjectId, QList<double>> &leftSDI){
    double min = -1;

    foreach (ObjectId t, taskCore.keys()){
        if(taskCore.find(t).value() != c){
            continue;
        }
        for (int i = 0; i < leftSDI.find(t).value().size(); i++) {
            if(left > leftSDI.find(t).value().at(i)){
                double a = left - leftSDI.find(t).value().at(i);
                if (min < 0 || min < a) {
                    min = a;
                }
            }
        }
    }

    return min;
}

double countNearestRight(CoreId c, double right, const QMap <ObjectId, CoreId> &taskCore, const QMap <ObjectId, QList<double>> &rightSDI){
    double min = -1;

    foreach (ObjectId t, taskCore.keys()){
        if(taskCore.find(t).value() != c){
            continue;
        }
        for (int i = 0; i < rightSDI.find(t).value().size(); i++) {
            if(right < rightSDI.find(t).value().at(i)){
                double a = rightSDI.find(t).value().at(i) - right;
                if (min < 0 || min < a) {
                    min = a;
                }
            }
        }
    }

    return min;
}

void countMesConstr(const QMap<CoreId, QList<double>> &coreProblemDur, const QMap<CoreId, QList<std::pair<double, double>>> &coreProblemInt,
                     const QMap <ObjectId, QList<double>> &leftSDI, const QMap <ObjectId, QList<double>> & rightSDI,
                     const QMap<ObjectId, Message*> &taskMesLeft, const QMap<ObjectId, Message*> &taskMesRight,
                     const QMap <ObjectId, CoreId> &taskCore, QMap<ObjectId, double> &mesExtraConstr,
                     const QMap <ObjectId, double> &messageDur,
                     const QMap<ObjectId, double> &maxMesDur){


    foreach(CoreId c, coreProblemDur.keys()){
        QList<std::pair<double, double>> ints = coreProblemInt.find(c).value();
        QList<double> durs = coreProblemDur.find(c).value();
        for (int i = 0; i < durs.size(); i++){

            //ïî-õîğîøåìó: ıòî äîëæåí áûòü ñïèñîê ÀÉÄÈ
            QList<ObjectId> taskLeft = findTaskBySDI(taskCore, leftSDI, c, ints.at(i).first);
            QList<ObjectId> taskRight = findTaskBySDI(taskCore, rightSDI, c, ints.at(i).second);
            QList<Message*> mLeft;
            bool isLeft = false;
            foreach(ObjectId t, taskLeft){
                if(taskMesLeft.contains(t)){
                    mLeft.append(taskMesLeft.find(t).value());
                    isLeft = true;
                }
            }
            QList<Message*> mRight;
            bool isRight = false;
            foreach(ObjectId t, taskRight){
                if(taskMesRight.contains(t)){
                    mRight.append(taskMesRight.find(t).value());
                    isRight = true;
                }
            }
            double delta = durs.at(i);

            //1 or 2 messages there
            QList<QList<Message*>> pairsMes;

            int maxPair = 0;
            float maxFreeSpace = 0;

            for (int k = 0; k < mLeft.size(); k++){
                if(messageDur.find(mLeft.at(k)->id()).value() - delta > -0.0000001){
                    QList<Message*> local;
                    local.append(mLeft.at(k));
                    pairsMes.append(local);
                    if (maxMesDur.find(mLeft.at(k)->id()).value() - messageDur.find(mLeft.at(k)->id()).value() - maxFreeSpace > -0.0000001){
                        maxPair = pairsMes.size();
                        maxFreeSpace = maxMesDur.find(mLeft.at(k)->id()).value() - messageDur.find(mLeft.at(k)->id()).value();
                    }
                }
                for (int m = 0; m < mRight.size(); m++){
                    if (messageDur.find(mLeft.at(k)->id()).value() + messageDur.find(mRight.at(m)->id()).value() - delta > -0.0000001){
                        QList<Message*> local;
                        local.append(mLeft.at(k));
                        local.append(mRight.at(m));
                        pairsMes.append(local);
                        if (maxMesDur.find(mLeft.at(k)->id()).value() - messageDur.find(mLeft.at(k)->id()).value() + maxMesDur.find(mRight.at(m)->id()).value() - messageDur.find(mRight.at(m)->id()).value() - maxFreeSpace > -0.0000001){
                            maxPair = pairsMes.size();
                            maxFreeSpace = maxMesDur.find(mLeft.at(k)->id()).value() - messageDur.find(mLeft.at(k)->id()).value() + maxMesDur.find(mRight.at(m)->id()).value() - messageDur.find(mRight.at(m)->id()).value();
                        }
                    }
                }
            }

            for (int k = 0; k < mRight.size(); k++){
                if(messageDur.find(mRight.at(k)->id()).value() - delta > -0.0000001){
                    QList<Message*> local;
                    local.append(mRight.at(k));
                    pairsMes.append(local);
                    if (maxMesDur.find(mRight.at(k)->id()).value() - messageDur.find(mRight.at(k)->id()).value() - maxFreeSpace > -0.0000001){
                        maxPair = pairsMes.size();
                        maxFreeSpace = maxMesDur.find(mRight.at(k)->id()).value() - messageDur.find(mRight.at(k)->id()).value();
                    }
                }
            }

            float sumOfDurs = 0;
            for (int k = 0; k < pairsMes.at(maxPair).size(); k++){
                sumOfDurs+=messageDur.find(pairsMes.at(maxPair).at(k)->id()).value();
            }

            for (int k = 0; k < pairsMes.at(maxPair).size(); k++){
                mesExtraConstr.insert(pairsMes.at(maxPair).at(k)->id(), std::max(mesExtraConstr.find(pairsMes.at(maxPair).at(k)->id()).value(),
                                                                                 delta*(messageDur.find(pairsMes.at(maxPair).at(k)->id()).value()/sumOfDurs)));
            }
        }
    }
}

void writeTaskSDIs(const QMap <ObjectId, QList<double>> &leftSDI, const QMap <ObjectId, QList<double>> &rightSDI, const QList<Task*> &tasks ){
    if (!loggerSDI.open(QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "?????? ??? ???????? ?????";
    }

    for (int i = 0; i < tasks.size(); i++){
        sdiStream<<"Task (first job) "<< QString::fromStdString(std::to_string((tasks.at(i)->id()).getId()))<<" : (leftSDI ; rightSDI) "<< leftSDI.find(tasks.at(i)->id()).value().first()
                <<" ; "<< rightSDI.find(tasks.at(i)->id()).value().first()<<"\n";
        qDebug()<<"Task (first job) "<< QString::fromStdString(std::to_string((tasks.at(i)->id()).getId()))<<" : (leftSDI ; rightSDI) "<< leftSDI.find(tasks.at(i)->id()).value().first()
               <<" ; "<< rightSDI.find(tasks.at(i)->id()).value().first()<<"\n";
    }


}

//воозвращать список разделов не вмещающихс¤ в свои —ƒ» и список проблемных интервалов????
// onlycheck = true - когда идет проверка в процессе МВГ, как только нашли проблему- сразу запрещаем этораспределение
bool PDCChecker::checkPDC(QMap <ObjectId, double> &messageDur, const QList<myTreeNode> &myList, ScheduleData *data, bool onlyCheck,
                  QMultiMap<ObjectId, QSet<ObjectId>> &partsPDC /*ïóñòîé â íà÷àëå- åãî òîëüêî âîçâğàùàåì*/, QMap<ObjectId, double> &mesConstr,
                         const QMap<ObjectId, double> &maxMesDur){
    QMap <ObjectId, CoreId> taskCore;
    QMap <ObjectId, QList<int>> partTasks;
    QMap<ObjectId, CoreId> partsSDI;
    QMap<ObjectId, Message*> taskMesLeft;
    QMap<ObjectId, Message*> taskMesRight;

    bool isProblem = false;
    //нужно поменять - сделать QMap>> - для каждой задачи, чей период меньше I нужно считать несколько раз

    QMap <ObjectId, QList<double>> leftSDI;
    QMap <ObjectId, QList<double>> rightSDI;

    QMap<CoreId, QList<double>> times;

    QList<Partition*> parts = data->partitions();
    QList<Task*> tasks = data->tasks();
    QList<CoreId> cores = data->cores();
    QList<Message*> messages = data->messages();

    QList<ObjectId> startTasks;
    QList<ObjectId> endTasks;

    QMap<ObjectId, TimeType> taskPeriod;

    QMap<CoreId, QList<std::pair<double, double>>> coreProblemInt;
    QMap<CoreId, QList<double>> coreProblemDur;

    taskCore = initTaskCore (myList, tasks);

    if(messageDur.size()<=0){
        initMesageDur(data, taskCore, messageDur);
    }


    for (int i = 0; i < parts.size(); i++){
        QList<int> l;
        partTasks.insert(parts.at(i)->id(),l);
    }

    for (int i = 0; i < tasks.size(); i++){
        TimeType T = tasks.at(i)->period();
        taskPeriod.insert(tasks.at(i)->id(), T);
        QList<double> l;
        l<<0;
        QList<double> r;
        r<<SchedInt/((int)SchedInt/T);
        leftSDI.insert(tasks.at(i)->id(), l);
        rightSDI.insert(tasks.at(i)->id(), r);
        startTasks.append(tasks.at(i)->id());
        endTasks.append(tasks.at(i)->id());
        partTasks.find(tasks.at(i)->partitionId()).value().append(i);
    }
    for (int i = 0; i < messages.size(); i++){
        startTasks.removeOne(messages.at(i)->receiverId());
        endTasks.removeOne(messages.at(i)->senderId());
    }

    for (int i = 0; i < startTasks.size(); i++){
        SDICounting::countLeftSDI(startTasks.at(i), messages, messageDur, leftSDI, taskCore, taskPeriod.find(startTasks.at(i)).value(),
                                  data, taskMesLeft);
    }
    for (int i = 0; i < endTasks.size(); i++){
        SDICounting::countRightSDI(endTasks.at(i), messages, messageDur, rightSDI, taskCore, taskPeriod.find(endTasks.at(i)).value(),
                                   data, taskMesRight);
    }

    if(!onlyCheck){
        writeTaskSDIs(leftSDI,rightSDI,tasks);
    }

    //times = qSort((leftSDI.values().toSet()+rightSDI.values().toSet()).toList());
    for (int i = 0; i < cores.size(); i++){
        QList<double> l;
        QList<std::pair<double, double>> p;
        QList<double> t;
        times.insert(cores.at(i), l);
        coreProblemInt.insert(cores.at(i),p);
        coreProblemDur.insert(cores.at(i),t);
    }

    for (int i = 0; i < tasks.size(); i++){
        ObjectId taskId = tasks.at(i)->id();
        CoreId coreId = taskCore.find(taskId).value();
        QList<double> l = leftSDI.find(taskId).value();
        QList<double> r = rightSDI.find(taskId).value();
        if (r.first()-l.first() < SDICounting::countTaskDur(taskId, taskCore, data)){
            if (onlyCheck) return false;
            partsSDI.insert(tasks.at(i)->partitionId(),coreId);
        }
        times.find(coreId).value().append(l);
        times.find(coreId).value().append(r);
    }
    if (partsSDI.size()!= 0){
        qDebug()<<"SDI failure\n";
        return false;
    }
    //найдем проблемные интервалы дл¤ всех ¤дер
    for (int i = 0; i < cores.size(); i++){
        QList<double> time = times.find(cores.at(i)).value().toSet().toList();
        qSort(time);
        for (int j = 0; j < time.size()-1; j++){
            for (int k = j+1; k < time.size(); k++){
                double start = time.at(j);
                double end = time.at(k);
                double max = end - start;
                QSet<ObjectId> parts;
                double sum = 0.0;
                for (int t = 0; t < tasks.size(); t++){
                    ObjectId taskId = tasks.at(t)->id();
                    // если работа приналлежит данному ¤дру и ее —ƒ» лежит внутри рассматриваемого интервала
                    //нужно мен¤ть проверку - учитывать несколько одинаковых работ
                    int count = 0;
                    if (taskCore.find(taskId).value() == cores.at(i)){
                        for (int e = 0; e < leftSDI.find(taskId).value().size(); e++){
                            if (leftSDI.find(taskId).value().at(e) >= start && leftSDI.find(taskId).value().at(e) < end
                                    && rightSDI.find(taskId).value().at(e) > start && rightSDI.find(taskId).value().at(e) <= end){
                                count++;
                                parts.insert(tasks.at(t)->partitionId());
                            }
                        }
                    }
                    sum += SDICounting::countTaskDur(taskId, taskCore, data)*count;
                }

                ProcessorType* procType = ProjectData::getProcessorType(cores.at(i).processorTypeId());
                sum += (parts.size()-1)*(procType->contextSwitch());
                if (sum >= max){
                    if (onlyCheck) return false;
                    isProblem = true;
                    coreProblemInt.find(cores.at(i)).value().append(std::make_pair(start,end));
                    coreProblemDur.find(cores.at(i)).value().append(sum-max);
                }
            }
        }
    }
    if (onlyCheck || !isProblem) return true;

    //считаем, насколько нужно уменьшить сообщени¤

    countMesConstr(coreProblemDur, coreProblemInt, leftSDI, rightSDI, taskMesLeft, taskMesRight, taskCore, mesConstr, messageDur, maxMesDur);

    for(int c = 0; c < cores.size(); c++){
        //partition - set of problem intervals with it
        QMap<ObjectId,QSet<int>> partInt;
        for (int p = 0; p < myList.size(); p++){
            if(myList.at(p).mCore!=cores.at(c)) continue;
            QSet<int> sp;
            for (int i = 0; i < coreProblemInt.find(cores.at(c)).value().size(); i++){
                double start = coreProblemInt.find(cores.at(c)).value().at(i).first;
                double end = coreProblemInt.find(cores.at(c)).value().at(i).second;
                double sum = 0;
                for (int t = 0; t < partTasks.find(myList.at(p).mPart).value().size(); t++){
                    Task* curTask = tasks.at(partTasks.find(myList.at(p).mPart).value().at(t));
                    int count = 0;
                    for (int e = 0; e < leftSDI.find(curTask->id()).value().size(); e++){
                        if (leftSDI.find(curTask->id()).value().at(e) >= start && leftSDI.find(curTask->id()).value().at(e) < end
                                && rightSDI.find(curTask->id()).value().at(e) > start && rightSDI.find(curTask->id()).value().at(e) <= end){
                            count++;
                        }
                    }
                    sum+=SDICounting::countTaskDur(curTask->id(), taskCore, data)*count;
                }
                if(sum >= coreProblemDur.find(cores.at(c)).value().at(i)){
                   //добавить этот раздел в множество разделов дл¤ данного проблемного интревала
                    sp.insert(i);
                    //We assume now, that we do not need to fix partitions if we have Constraints 'Not together'
                   // fixedParts.insert(myList.at(p).mPart, myList.at(p).mCore);
                }
            }
            partInt.insert(myList.at(p).mPart,sp);
        }
        //for each patition
        // for each problem Int for it
        //find other partions with same core and same problem int
        //add this part + set of found parts to partsPDC
        for (int k = 0; k < partInt.keys().size(); k++){
            ObjectId part = partInt.keys().at(k);
            QSet<int>::const_iterator problemInts = partInt.find(part).value().begin();
            //check if the parts rom the prohibition already in solution - if no - it is Ok, we can go futher
            while (problemInts!= partInt.find(part).value().end()){
               QSet<ObjectId> notWithPart;
               for (int m = 0; m < partInt.keys().size(); m++){
                   ObjectId checkP = partInt.keys().at(m);
                   if (part == checkP) continue;
                   if (partInt.find(checkP).value().contains(*problemInts)){
                       notWithPart.insert(checkP);
                   }
               }
               if(notWithPart.size()!=0){
                   partsPDC.insert(part,notWithPart);
               }
               problemInts++;
            }
        }
//        while(true){
//            int max = 0;
//            double maxC = 0.0;
//            ObjectId maxP;
//            QSet<int> maxI;
//            for (int o = 0; o < partInt.keys().size(); o++){
//                if (partInt.find(partInt.keys().at(o)).value().size()>=max && partInt.find(partInt.keys().at(o)).value().size()!=0){
//                    double load = data->partLoad(partInt.keys().at(o),cores.at(c));
//                    if (max == 0){
//                        maxC = load;
//                    }
//                    if(load <= maxC){
//                        max = partInt.find(partInt.keys().at(o)).value().size();
//                        maxP = partInt.keys().at(o);
//                        maxI = QSet<int>(partInt.find(maxP).value());
//                        maxC = load;
//                    }
//                }
//            }
//            if (max == 0) break;
//            partsPDC.insert(maxP,cores.at(c));
//            //fixedParts.remove(maxP);
//            for (int o = 0; o < partInt.keys().size(); o++){
//                partInt.find(partInt.keys().at(o)).value().subtract(maxI);
//            }
//        }
        //сделать анализ полученных списков разделов и ограничить прив¤зку

    }

    return false;
}

}
