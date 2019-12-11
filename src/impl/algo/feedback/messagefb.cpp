#include "messagefb.h"
#include "bindalgobranchandbound.h"

#define EPS 0.001

namespace A653 {

//считаем сумму всех (size*period) сообщений входящих и исходящих из модуля, невключая упавших сообщений
void MessageFB::countMesFB(ScheduleData* data, const QList<Message*> &failedMes, QMap<ObjectId,double> &moduleThr,
                           const QMap<ObjectId, CoreId> &taskCore, const QList<ObjectId> &failedMod){
    QList<Message*> messages = data->messages();
    QMap<ObjectId,double> curThr;
    QMap<ObjectId,bool> troubleMod;

    foreach(Module* m, data->modules()){
        curThr.insert(m->id(),0);
        troubleMod.insert(m->id(), false);
//        minMes.insert(m->id(), 10000000000000000000);
    }
    foreach(Message* mes, messages){
        ObjectId mrcv = taskCore.find(mes->receiverId()).value().moduleId;
        ObjectId msnd = taskCore.find(mes->senderId()).value().moduleId;
        if(msnd == mrcv){
            continue;
        }
        if(failedMes.contains(mes)){
//            if(minMes.find(mrcv).value()> (mes->size())*(mes->sender()->frequency())){
//                minMes.insert(mrcv, (mes->size())*(mes->sender()->frequency()));
//            }
//            if(minMes.find(msnd).value()> (mes->size())*(mes->sender()->frequency())){
//                minMes.insert(msnd, (mes->size())*(mes->sender()->frequency()));
//            }
            if (failedMod.contains(mrcv))
                troubleMod.insert(mrcv, true);
            if (failedMod.contains(msnd))
                troubleMod.insert(msnd, true);
        }
        curThr.insert(mrcv, curThr.find(mrcv).value()+ (mes->size())*(mes->sender()->frequency()));
        curThr.insert(msnd, curThr.find(msnd).value()+ (mes->size())*(mes->sender()->frequency()));
    }

    foreach(Module* m, data->modules()){
       if(troubleMod.find(m->id()).value()){
          // moduleThr.insert(m->id(),curThr.find(m->id()).value() - minMes.find(m->id()).value());
          // moduleThr.insert(m->id(),curThr.find(m->id()).value() - std::min(minMes.find(m->id()).value(), 100.0));
          moduleThr.insert(m->id(),curThr.find(m->id()).value() - EPS);
       }
    }
}
}

