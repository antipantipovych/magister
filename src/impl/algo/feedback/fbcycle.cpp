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
                        const QMap<ObjectId, CoreId>  &taskCore, QMap<ObjectId, int> &curMesConstr, ScheduleData* data){
        curSum+= (double)current->duration(taskCore.find(current->id()).value().processorTypeId());
        bool isEndTask = true;
        foreach (Message* m, data->messages()){
            if (m->senderId()==current->id()){
                curPathMes.append(m);
                countMesConstr(m->receiver(), curPathMes, curSum, taskCore, curMesConstr, data);
                isEndTask = false;
            }
        }
        if(isEndTask){
            double space = (double)current->period() - curSum;
            double sizes = 0;
            for (int i = 0; i < curPathMes.size(); i++){
                Message* m = curPathMes.at(i);
                if (taskCore.find(m->senderId()).value().moduleId != taskCore.find(m->receiverId()).value().moduleId){
                    sizes += m->size();
                }
                else{
                    space -= m->DefaultMDur;
                }
            }

            for (int i = 0; i < curPathMes.size(); i++){
                int partSpace ;
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

    QMap <ObjectId, int> countMesMaxDur(ScheduleData* data, const QMap<ObjectId, CoreId>  &taskCore,
                                        const QMap<ObjectId, double> &mesConstr, const QMap <ObjectId, double> &mesDur){
        QMap <ObjectId, int> d;
        QList<Message*> mes = data->messages();
        QList<Task*> tasks = data->tasks();
        QList<Task*> startTasks = data->tasks();

        foreach (Message* m, mes){
            startTasks.removeOne(m->receiver());
        }

        foreach (Task* t, startTasks){
            QList<Message*> curPathMes;
            double sum = 0;
            countMesConstr(t, curPathMes, sum, taskCore, d, data);
        }

        foreach (Message* m, mes){
            if(mesConstr.contains(m->id())){
                d.find(m->id()).value()= mesDur.find(m->id()).value()-mesConstr.find(m->id()).value();
            }
        }

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
            qDebug() << "Ошибка при открытии файла";
        }

//        std::string filename = Project::project()->lastSavedStorage()->name.toStdString() + "VL";

        std::string filename= (inputFile.left(inputFile.length()-4)).toStdString()+"_afdx.xml";
        qDebug()<<QString::fromStdString(filename);
        std::string afdxFile= "../../"+(inputFile.mid(3,(inputFile.length()-7))).toStdString()+"_afdx.xml";
        std::replace(afdxFile.begin(), afdxFile.end(),'\\', '/');

        //делаем первичную привязку при помощи МВГ
        QMultiMap<ObjectId, CoreId> extraConstr;
        QMap<ObjectId, CoreId> fixedParts;
        QMap<ObjectId, CoreId> solution;
        QMap <ObjectId, double> mesDur;
        QList<myTreeNode> binding;

        BindAlgoBranchNB mvg (extraConstr, fixedParts, schedule);
        mvg.makeBinding(schedule);

        if(mvg.fail) {
            logStream<<"Init mvg failed\n";
            qDebug()<<"Init mvg failed\n";
            return;
        }

        binding=mvg.mOpt;

        bool pdcResult = true;


        QMap<ObjectId, double> mesConstr;

        QMap<ObjectId,double> moduleThrConstr;
        foreach(Module* m, schedule->data()->modules()){
            moduleThrConstr.insert(m->id(),MaxThroughput);
        }

        while(true){
            initFixedParts(binding, solution, fixedParts);
            //строим более удобную структуру решения привязки: задача - ядро
            QMap<ObjectId, CoreId> taskCore = PDCChecker::initTaskCore(binding, schedule->data()->tasks());

            //считаем ограничения на максимальную длительность передачи сообщений
            QMap<ObjectId, int> mesMaxDur;
            mesMaxDur = countMesMaxDur(schedule->data(), taskCore, mesConstr, mesDur);

            //сторим виртуальные каналы
            XMLProcessor::create_afdx_xml(QString::fromStdString(filename), schedule->data(), solution, mesMaxDur);

            std::string command = "C:/Windows/sysnative/bash.exe -c '../../AFDX_Designer/AFDX_Designer/algo/AFDX_DESIGN " + afdxFile + " a'";
            qDebug()<<command.c_str();
            system(command.c_str());

            // считаем фактические длительности сообщений
            QList<Message*> failedMes;
            mesDur.clear();
            XMLProcessor::get_vl_response(filename, schedule->data(), mesDur,  failedMes, taskCore);
            if (failedMes.size()!= 0){
                 logStream<<"there were FAILED VLs";
                 qDebug()<<"there were FAILED VLs";
                 MessageFB::countMesFB(schedule->data(), failedMes, moduleThrConstr, taskCore);
                 mesConstr.clear();
             //TODO: при сопряжении FB нужно будет учитывать fixedParts
                 if(extraConstr.size() == 0){
                    fixedParts.clear();  //хотя бы так
                 }
                 BindAlgoBranchNB mvgLocal (extraConstr, fixedParts, moduleThrConstr);
                 mvgLocal.makeBinding(schedule);
                 if(mvgLocal.fail) {
                     logStream<<"mvg failed\n";
                     qDebug()<<"mvg failed\n";
                     break;
                 }
                 binding = mvgLocal.mOpt;
            }
            else{
                //feedback с PDC на привязку
                QMultiMap<ObjectId, CoreId> localExtraConstr;
                pdcResult = PDCChecker::checkPDC(mesDur, binding, schedule->data(), false, localExtraConstr, fixedParts, mesConstr);
                if (pdcResult) {
                    logStream<<"pdcCheck was successful\n";
                    qDebug()<<"pdcCheck was successful\n";
                    break;
                }
                if (!pdcResult && localExtraConstr.size() == 0){
                    logStream<<"during the last pdcCheck no new constraints were added, but the result is unsuccessful"<<"\n"<<"Use other feedback\n";
                    qDebug()<<"during the last pdcCheck no new constraints were added, but the result is unsuccessful"<<"\n"<<"Use other feedback\n";
                    continue;
                    //пересчет AFDX,пересчет PDC, не меняется extraConstr
                    //особо ничего делать не надо - когда упадет AFDX - пойдет перераспределение разделов - пойдут новые циклы
                }
                else{
                    logStream<<"pdcCheck unsuccessful, new Constr added\n";
                    qDebug()<<"pdcCheck unsuccessful, new Constr added\n";
                    //тут тоже нужно сначала попробовать без новых ограничений прогнать изменение сообщений
                    QMap<ObjectId, int> localMesMaxDur;
                    localMesMaxDur = countMesMaxDur(schedule->data(), taskCore, mesConstr, mesDur);

                    XMLProcessor::create_afdx_xml(QString::fromStdString(filename), schedule->data(), solution, localMesMaxDur);

                    std::string command = "C:/Windows/sysnative/bash.exe -c '../../AFDX_Designer/AFDX_Designer/algo/AFDX_DESIGN " + afdxFile + " a'";
                    qDebug()<<command.c_str();
                    system(command.c_str());

                    // считаем фактические длительности сообщений
                    mesDur.clear();
                    XMLProcessor::get_vl_response(filename, schedule->data(), mesDur,  failedMes, taskCore);
                    if (failedMes.size()!= 0){
                         logStream<<"there were FAILED VLs";
                         qDebug()<<"there were FAILED VLs";
                         extraConstr.unite(localExtraConstr);
                         //break;
                         //TODO
                         //feedback на привязку: что воообще для этого нужно
                    }
                    else{
                    mesConstr.clear();
                    QMultiMap<ObjectId, CoreId> newlocalExtraConstr;
                    pdcResult = PDCChecker::checkPDC(mesDur, binding, schedule->data(), false, newlocalExtraConstr, solution, mesConstr);
                    if(pdcResult){
                        //если было достаточно поменять длительности сообщений-строим расписание окон
                        break;
                    }
                    else{
                        //иначе пробуем полученные на PDC "до" доп ограничения
                        extraConstr.unite(localExtraConstr);
                    }}
                }

                mesConstr.clear();
                BindAlgoBranchNB mvgLocal (extraConstr, fixedParts, schedule);
                mvgLocal.makeBinding(schedule);
                if(mvgLocal.fail) {
                    logStream<<"mvg failed\n";
                    qDebug()<<"mvg failed\n";
                    break;
                }
                binding = mvgLocal.mOpt;
            }
        }

        //инициализация coreForPArt последней корректной привязкой Schedule::bindPartition
        //по-идее после последнего распределения эти данные и так лежат в Schedule

        //инициализация setChannelDuration для каждого сообщения:
        setMesDurations(schedule, schedule->data()->messages(), mesDur);
        ProjectImpl::algorithms()->schedAlgo()->makeSchedule(schedule);

    }
}
