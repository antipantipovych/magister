#include "fbcycle.h"

#include <QDebug>

#include "projectdata.h"
#include "projectimpl.h"
#include "scheduledata.h"
#include "algofactory.h"
#include "bindalgobranchandbound.h"
#include "pdcchecker.h"
#include "xmlprocessor.h"
#include "messagefb.h"
#include "qprocess.h"
#include <QDir>
#include <fstream>

namespace A653 {

 QFile logger("fb_log.txt");
 QTextStream logStream(&logger);

 std::string filePath = "../../saprcliA/build-sapr-console-Desktop_Qt_5_5_1_MinGW_32bit-Release/";

    void countMesConstr(Task* current, QList<Message*> curPathMes, double curSum,
                        const QMap<ObjectId, CoreId>  &taskCore, QMap<ObjectId, double> &curMesConstr, ScheduleData* data,
                        const QMap<ObjectId, double> &mesConstr, const QMap <ObjectId, double> &mesDur,
                        QMap<ObjectId, double> &minChainMessage){
        curSum+= (double)current->duration(taskCore.find(current->id()).value().processorTypeId());
        bool isEndTask = true;
        foreach (Message* m, data->messages()){
            if (m->senderId()==current->id()){
                curPathMes.append(m);
                countMesConstr(m->receiver(), curPathMes, curSum, taskCore, curMesConstr, data, mesConstr, mesDur, minChainMessage);
                isEndTask = false;
            }
        }
        if(isEndTask){
            double space = (double)current->period() - curSum;
            double sizes = 0;
            for (int i = 0; i < curPathMes.size(); i++){
                Message* m = curPathMes.at(i);
                if (taskCore.find(m->senderId()).value().moduleId != taskCore.find(m->receiverId()).value().moduleId){
                    if(!mesConstr.contains(m->id())){
                        sizes += m->size();
                    }
                    else {
                        curMesConstr.insert(m->id(), mesDur.find(m->id()).value()-mesConstr.find(m->id()).value());
                        space -= mesDur.find(m->id()).value()-mesConstr.find(m->id()).value();
                    }
                }
                else{
                    space -= m->DefaultMDur;
                }
            }

            for (int i = 0; i < curPathMes.size(); i++){
                //we should not take into the account the Messages, which were choosen for additional constraints
                //we know their fixed maxDuration

                minChainMessage.insert(curPathMes.at(i)->id(), std::min(minChainMessage.find(curPathMes.at(i)->id()).value(),space));

                if(mesConstr.contains(curPathMes.at(i)->id())){
                   continue;
                }
                double partSpace ;
                Message* m = curPathMes.at(i);
                if (taskCore.find(m->senderId()).value().moduleId != taskCore.find(m->receiverId()).value().moduleId){
                    partSpace = ceil((double)space/sizes*(m->size()));
                }
                else {
                    partSpace = m->DefaultMDur;
                }
                if(curMesConstr.contains(m->id())){
                    curMesConstr.insert(m->id(),std::min(curMesConstr.find(m->id()).value(),partSpace));
                }
                else{
                    curMesConstr.insert(m->id(), partSpace);
                }

            }
        }
    }

    QMap <ObjectId, double> countMesMaxDur(ScheduleData* data, const QMap<ObjectId, CoreId>  &taskCore,
                                        const QMap<ObjectId, double> &mesConstr, const QMap <ObjectId, double> &mesDur,
                                        QMap<ObjectId, double> &minChainMessage){
        QMap <ObjectId, double> d;
        QList<Message*> mes = data->messages();
        QList<Task*> startTasks = data->tasks();

        foreach (Message* m, mes){
            startTasks.removeOne(m->receiver());
        }

        foreach (Task* t, startTasks){
            QList<Message*> curPathMes;
            double sum = 0;
            countMesConstr(t, curPathMes, sum, taskCore, d, data, mesConstr, mesDur, minChainMessage);
        }

//        foreach (Message* m, mes){
//            if(mesConstr.contains(m->id())){
//                d.find(m->id()).value()= mesDur.find(m->id()).value()-mesConstr.find(m->id()).value();
//            }
//        }

        return d;
    }

    void initFixedParts(const QList<myTreeNode> &binding, QMap<ObjectId, CoreId> &solution, QMap<ObjectId, CoreId> &fixed){
        foreach(myTreeNode node, binding){
            //fixed.insert(node.mPart, node.mCore);
            solution.insert(node.mPart, node.mCore);
        }
    }

    void setMesDurations(Schedule* schedule, QList<Message*> mes, QMap <ObjectId, double> mesDur){
        foreach(Message* m, mes){
            if (schedule->coreForTask(m->senderId()).moduleId == schedule->coreForTask(m->receiverId()).moduleId){
                m->setModuleDuration( TimeType (mesDur.find(m->id()).value()));
            }
            else{
                m->setChannelDuration( TimeType (mesDur.find(m->id()).value()));
            }
        }
    }

    void FBCycle::execute (Schedule* schedule, QString inputFile){
        if (!logger.open(QIODevice::Append | QIODevice::Text))
        {
            qDebug() << "?????? ??? ???????? ?????";
        }

//        std::string filename = Project::project()->lastSavedStorage()->name.toStdString() + "VL";

        std::string filename= (inputFile.left(inputFile.length()-4)).toStdString()+"_afdx.xml";
        qDebug()<<QString::fromStdString(filename);
        std::string afdxFile= "../../"+(inputFile.mid(3,(inputFile.length()-7))).toStdString()+"_afdx.xml";
        std::replace(afdxFile.begin(), afdxFile.end(),'\\', '/');

        //?????? ????????? ???????? ??? ?????? ???
        QMultiMap<ObjectId, CoreId> extraConstr;
        QMultiMap<ObjectId, QSet<ObjectId>> notTogether;
        QMap<ObjectId, CoreId> fixedParts;
        QMap<ObjectId, CoreId> solution;
        QMap <ObjectId, double> mesDur;
        QList<myTreeNode> binding;

        BindAlgoBranchNB mvg (extraConstr, notTogether, fixedParts, schedule);
        mvg.makeBinding(schedule);

        if(mvg.fail) {
            logStream<<"Init mvg failed\n";
            qDebug()<<"Init mvg failed\n";
            return;
        }

        binding=mvg.mOpt;

        bool pdcResult = true;
        bool pdc_vl_mvg = false;

        QMap<ObjectId, double> mesConstr;

        QMap<ObjectId,double> moduleThrConstr;
        foreach(Module* m, schedule->data()->modules()){
            moduleThrConstr.insert(m->id(),MaxThroughput);
        }

        while(true){
//We do not need fixed parts more
//            initFixedParts(binding, solution, fixedParts);
            //?????? ????? ??????? ????????? ??????? ????????: ?????? - ????
            QMap<ObjectId, CoreId> taskCore = PDCChecker::initTaskCore(binding, schedule->data()->tasks());

            //??????? ??????????? ?? ???????????? ???????????? ???????? ?????????
            QMap<ObjectId, double> mesMaxDur;
            QMap<ObjectId, double> minChainMessage;
            mesConstr.clear();
            mesMaxDur = countMesMaxDur(schedule->data(), taskCore, mesConstr, mesDur, minChainMessage);

            //?????? ??????????? ??????
            XMLProcessor::create_afdx_xml_triangle(QString::fromStdString(filename), schedule->data(), solution, mesMaxDur);

            std::string command = "C:/Windows/sysnative/bash.exe -c '../../AFDX_Designer/AFDX_Designer/algo/AFDX_DESIGN " + afdxFile + " a'";
            qDebug()<<command.c_str();
            system(command.c_str());

            // ??????? ??????????? ???????????? ?????????
            QList<Message*> failedMes;
            QList<ObjectId> troubleMod;
            mesDur.clear();
            XMLProcessor::get_vl_response(filename, schedule->data(), mesDur,  failedMes, taskCore, troubleMod);
            if (failedMes.size()!= 0){
                 logStream<<"there were FAILED VLs";
                 qDebug()<<"there were FAILED VLs";
                 if (troubleMod.size() == 0){
                     logStream<<"there were FAILED only INNER Links";
                     qDebug()<<"there were FAILED only INNER Links";
                     return;
                 }

                 //if this branch associated with the case when we started a FB gtom the PDC - we would not rebuild the mvg to
                 //align new AFDX conatraints
                 if(!pdc_vl_mvg){
                    // countMesFB - counts new moduleTHRConstr
                    MessageFB::countMesFB(schedule->data(), failedMes, moduleThrConstr, taskCore, troubleMod);
                 }

                 BindAlgoBranchNB mvgLocal (extraConstr, notTogether, fixedParts, moduleThrConstr);
                 mvgLocal.makeBinding(schedule);
                 if(mvgLocal.fail) {
                     logStream<<"mvg failed\n";
                     qDebug()<<"mvg failed\n";
                     break;
                 }
                 else{
                    mesDur.clear();
                    mesConstr.clear();
                    binding = mvgLocal.mOpt;
                    pdc_vl_mvg = false;
                    //after this we are going to the beginning of the cycle
                 }
            }
            else{
                //feedback ? PDC ?? ????????
                //QMultiMap<ObjectId, QSet<ObjectId>> localNotTogether;// == noTogether
                pdcResult = PDCChecker::checkPDC(mesDur, binding, schedule->data(), false, notTogether, mesConstr, mesMaxDur);
                if (pdcResult) {
                    logStream<<"pdcCheck was successful\n";
                    qDebug()<<"pdcCheck was successful\n";
                    break;
                }
                else{
                   // notTogether.unite(localNotTogether);
                   logStream<<"pdcCheck unsuccessful, new Constr added\n";
                    qDebug()<<"pdcCheck unsuccessful, new Constr added\n";
                    pdc_vl_mvg = true;
//                    //??? ???? ????? ??????? ??????????? ??? ????? ??????????? ???????? ????????? ?????????
//                    QMap<ObjectId, double> localMesMaxDur;
//                    minChainMessage.clear();
//                    localMesMaxDur = countMesMaxDur(schedule->data(), taskCore, mesConstr, mesDur, minChainMessage);

//                    XMLProcessor::create_afdx_xml_triangle(QString::fromStdString(filename), schedule->data(), solution, localMesMaxDur);

//                    std::string command = "C:/Windows/sysnative/bash.exe -c '../../AFDX_Designer/AFDX_Designer/algo/AFDX_DESIGN " + afdxFile + " a'";
//                    qDebug()<<command.c_str();
//                    system(command.c_str());

//                    // ??????? ??????????? ???????????? ?????????
//                    mesDur.clear();
//                    XMLProcessor::get_vl_response(filename, schedule->data(), mesDur,  failedMes, taskCore, troubleMod);
//                    if (failedMes.size()!= 0){
//                         logStream<<"there were FAILED VLs";
//                         qDebug()<<"there were FAILED VLs";
//                         if (troubleMod.size() == 0){
//                             logStream<<"there were FAILED only INNER Links";
//                             qDebug()<<"there were FAILED only INNER Links";
//                             return;
//                         }
//                         else{
//                         //FB PDC -> MVG
//                             pdc_vl_mvg = true;
//                         }
//                    }
//                    else{
//                        mesConstr.clear();
//                        //QMultiMap<ObjectId, QSet<ObjectId>> newlocalNotTogether; // == notTogether
//                        pdcResult = PDCChecker::checkPDC(mesDur, binding, schedule->data(), false, notTogether, mesConstr, localMesMaxDur);
//                        if(pdcResult){
//                            //???? ???? ?????????? ???????? ???????????? ?????????-?????? ?????????? ????
//                            logStream<<"New PDC CHeck succesful";
//                            qDebug()<<"New PDC CHeck succesful";
//                            break;
//                        }
//                        else{
//                            //????? ??????? ?????????? ?? PDC "??" ??? ???????????

//                        }
//                    }
                }

//                mesDur.clear();
//                mesConstr.clear();
//         //need to clear notTogether MAYBE?
//                BindAlgoBranchNB mvgLocal (extraConstr, notTogether, fixedParts, schedule);
//                mvgLocal.makeBinding(schedule);
//                if(mvgLocal.fail) {
//                    logStream<<"mvg failed\n";
//                    qDebug()<<"mvg failed\n";
//                    break;
//                }
//                binding = mvgLocal.mOpt;
            }
        }

        //????????????? coreForPArt ????????? ?????????? ????????? Schedule::bindPartition
        //??-???? ????? ?????????? ????????????? ??? ?????? ? ??? ????? ? Schedule

        //????????????? setChannelDuration ??? ??????? ?????????:
        setMesDurations(schedule, schedule->data()->messages(), mesDur);
        ProjectImpl::algorithms()->schedAlgo()->makeSchedule(schedule);

    }
}
