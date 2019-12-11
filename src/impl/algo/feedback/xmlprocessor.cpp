
#include "xmlprocessor.h"
#include "qdom.h"
#include "qstring.h"
#include "qfile.h"

#define CAPACITY 40

namespace A653 {

void XMLProcessor::create_afdx_xml (QString fileName, ScheduleData * data, const QMap<ObjectId, CoreId> &partCore,
                                    const QMap<ObjectId, int> &mesMaxDur){
     QDomDocument doc;
     QDomProcessingInstruction instr = doc.createProcessingInstruction(
                        "xml", "version='1.0'");
     doc.appendChild(instr);
     QDomElement  domElement = doc.createElement("afdxxml");
     domElement.setAttribute("name", "antip test project");
     doc.appendChild(domElement);

     QDomElement resources = doc.createElement("resources");
     domElement.appendChild(resources);

     int resNumber = 0;
     int portsNumber = 0;
     for (int i = 0; i < data->modules().size(); i++){
         resNumber++;
         portsNumber++;
         QDomElement module = doc.createElement("endSystem");
         module.setAttribute("name", QString::fromStdString("endSystem"+std::to_string(i+1)));
         module.setAttribute("number", QString::number(resNumber));
         module.setAttribute("ports", QString::number(portsNumber));
         module.setAttribute("x", QString::number(10));
         module.setAttribute("y", QString::number(10));
         resources.appendChild(module);
     }

     QDomElement sw = doc.createElement("switch");
     sw.setAttribute("name","switch1");
     resNumber++;
     sw.setAttribute("number",QString::number(resNumber));
     sw.setAttribute("x", QString::number(10));
     sw.setAttribute("y", QString::number(10));
     portsNumber++;
     QString swPorts = QString::number(portsNumber);
     for (int i = 1; i < data->modules().size(); i++){
         portsNumber++;
         swPorts+=","+QString::number(portsNumber);
     }
     sw.setAttribute("ports", swPorts);
     resources.appendChild(sw);

     for (int i = 0; i < data->modules().size(); i++){
         QDomElement link = doc.createElement("link");
         link.setAttribute("capacity", CAPACITY);
         link.setAttribute("fromType", QString::number(0));
         link.setAttribute("toType", QString::number(0));
         link.setAttribute("from",QString::number(i+1));
         link.setAttribute("to",QString::number(i+1+data->modules().size()));
         link.setAttribute("faild", QString::number(0));
         resources.appendChild(link);
     }

     QMap<ObjectId, int> partNum;

     for (int i = 0; i < partCore.keys().size(); i++){
         resNumber++;
         QDomElement part = doc.createElement("partition");
         part.setAttribute("number", QString::number(resNumber));
         part.setAttribute("x", QString::number(10));
         part.setAttribute("y", QString::number(10));
         part.setAttribute("name", QString::fromStdString("partition"+std::to_string(i+1)));
         part.setAttribute("connectedTo", QString::number(partCore.find(partCore.keys().at(i)).value().moduleId.getId()+1));
         resources.appendChild(part);
         partNum.insert(partCore.keys().at(i),resNumber);
     }

     QDomElement vl = doc.createElement("virtualLinks");
     domElement.appendChild(vl);

     QDomElement dfs = doc.createElement("dataFlows");
     domElement.appendChild(dfs);

     for (int i = 0; i < data->messages().size(); i++){
         Message * m = data->messages().at(i);
         QDomElement dataFlow = doc.createElement("dataFlow");
         dataFlow.setAttribute("vl", "None");
         dataFlow.setAttribute("jMax", QString::number(0));
         dataFlow.setAttribute("id", QString::fromStdString("Data Flow "+std::to_string(i+1)));
         dataFlow.setAttribute("msgSize", QString::number(m->size()));
         dataFlow.setAttribute("period", QString::number(m->sender()->period()));
         dataFlow.setAttribute("source", QString::number(partNum.find(m->sender()->partitionId()).value()));
         dataFlow.setAttribute("dest", QString::number(partNum.find(m->receiver()->partitionId()).value()));
         dataFlow.setAttribute("tMax", QString::number(mesMaxDur.find(m->id()).value()));
         dfs.appendChild(dataFlow);
     }

     QFile file;
     file.setFileName(fileName);
     if(file.open(QIODevice::WriteOnly)) {
         QTextStream(&file) << doc.toString();
         file.close();
     }
}

void XMLProcessor::get_vl_response (std::string fileName, ScheduleData * data, QMap <ObjectId, double> &messageDur, QList<Message*> &failedMes,
                                    const QMap<ObjectId, CoreId> &taskCore, QList<ObjectId> &troubleMod){

    QDomDocument xmlBOM;
    QFile f(QString::fromStdString(fileName));
    if (!f.open(QIODevice::ReadOnly ))
    {
        // Error while loading file
        qDebug()<< "Error while loading file";
        return;
    }
    // Set data into the QDomDocument before processing
    xmlBOM.setContent(&f);
    f.close();

    QDomElement root=xmlBOM.documentElement();

    QDomNodeList rootChild = root.childNodes();

    QDomElement dfs = rootChild.at(2).toElement();
    QDomElement vls = rootChild.at(1).toElement();
    QDomNodeList links = root.elementsByTagName("link");

    //We are finding only 'out' links (connected to tne module)
    //There also may be 'inner' failed links
    for (int i = 0; i < data->modules().size(); i++){
       for (int j = 0; j < links.size(); j++){
           //Find the link associated with current module (assume, that there is only one link and it's from port == i+1)
           if (links.at(j).toElement().attribute("from").toInt() == i+1){
               if (links.at(j).toElement().attribute("failed").toInt() == 1){
                   troubleMod.append(data->modules().at(i)->id());
               }
           }
       }
    }

    for (int i = 0; i < dfs.childNodes().size(); i++){

        Message* curMes = data->messages().at(i);
        if(taskCore.find(curMes->senderId()).value().moduleId == taskCore.find(curMes->receiverId()).value().moduleId){
            messageDur.insert(curMes->id(), curMes->DefaultMDur);
        }

        QDomElement df = dfs.childNodes().at(i).toElement();
        QString vlNum = df.attributeNode("vl").value();
        int msgSize = df.attributeNode("msgSize").value().toInt();
        qDebug()<<vlNum<<'\n';
        for (int j = 0; j < vls.childNodes().size(); j++){
            QDomElement vl = vls.childNodes().at(j).toElement();
            if (vl.attributeNode("number").value() == vlNum){
                int bag = vl.attributeNode("bag").value().toInt();
                int rt = vl.attributeNode("responseTime").value().toInt();
                int lmax = vl.attributeNode("lmax").value().toInt();
                int framesNum = ceil((double)msgSize/lmax);

                qDebug()<<bag<<' '<<rt<<' '<<lmax<<' '<<framesNum<<'\n';

                double dur = ((double)rt+ (double)(framesNum - 1) *bag * 1000)/1000;
                messageDur.insert(curMes->id(), dur );
                break;
            }
        }
        if(!messageDur.contains(curMes->id())){
            failedMes.append(data->messages().at(i));
        }
    }
}


}
