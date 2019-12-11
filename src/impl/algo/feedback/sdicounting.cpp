#include "sdicounting.h"

#include <iostream>
#include <string>
#include "math.h"

#include <QDebug>
#include <QTime>
#include <QVector>
#include <QFile>

#include "scheduledata.h"
#include "projectdata.h"

namespace A653 {

double SDICounting::countTaskDur(const ObjectId &taskId, const QMap <ObjectId, CoreId> taskCore, ScheduleData *data) {
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

void SDICounting::countLeftSDI(const ObjectId &prevTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                  QMap <ObjectId, QList<double>> &leftSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data,
                               QMap<ObjectId, Message*> &leftMes){
    double prevDur = countTaskDur(prevTask, taskCore, data);
    for (int i = 0; i < messages.size(); i++){
        if (messages.at(i)->senderId()== prevTask){

            if(leftSDI.find(messages.at(i)->receiverId()).value().first() < leftSDI.find(prevTask).value().first()+prevDur+messageDur.find(messages.at(i)->id()).value()){
                leftSDI.find(messages.at(i)->receiverId()).value().first() = leftSDI.find(prevTask).value().first()+prevDur+messageDur.find(messages.at(i)->id()).value();
                leftMes.insert(messages.at(i)->receiverId(), messages.at(i));
            }

            countLeftSDI(messages.at(i)->receiverId(), messages, messageDur, leftSDI, taskCore, T, data, leftMes);
        }
    }
    for (int i = 1; i < (int)SchedInt/T; i++){
        leftSDI.find(prevTask).value().append(leftSDI.find(prevTask).value().first()+i*T);
    }
}

void SDICounting::countRightSDI(const ObjectId &nextTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                  QMap <ObjectId, QList<double>> &rightSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data,
                                QMap<ObjectId, Message*> &rightMes){
    double nextDur = countTaskDur(nextTask, taskCore, data);
    for (int i = 0; i < messages.size(); i++){
        if (messages.at(i)->receiverId()== nextTask){
            if(rightSDI.find(messages.at(i)->senderId()).value().first() > rightSDI.find(nextTask).value().first()- nextDur - messageDur.find(messages.at(i)->id()).value()){
                rightSDI.find(messages.at(i)->senderId()).value().first()= rightSDI.find(nextTask).value().first()- nextDur - messageDur.find(messages.at(i)->id()).value();
                rightMes.insert(messages.at(i)->senderId(), messages.at(i));
            }
            countRightSDI(messages.at(i)->senderId(), messages, messageDur, rightSDI, taskCore, T,  data, rightMes);
        }
    }
    for (int i = 1; i < SchedInt/T; i++){
        rightSDI.find(nextTask).value().append(rightSDI.find(nextTask).value().first()+i*T);
    }
}

}
