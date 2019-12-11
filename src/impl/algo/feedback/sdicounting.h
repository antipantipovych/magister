#ifndef SDICOUNTING
#define SDICOUNTING

#include "interfaces.h"
#include "objects.h"
#include "project.h"
#include "scheduledata.h"

namespace A653 {

//static constexpr double MIN_MES_DUR = 10;//���� ����� ������������ �������� ��������� �� ���������� ������?
static constexpr double SchedInt = 10000; //����� ����������
//procType->contextSwitch(); - ����� ����� ���
static constexpr double CS_END = 1000; //������������ CS - ����� ������

class SDICounting
{
public:

    static double countTaskDur(const ObjectId &taskId, const QMap <ObjectId, CoreId> taskCore, ScheduleData *data);

    static void countLeftSDI(const ObjectId &prevTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                      QMap <ObjectId, QList<double>> &leftSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data,
                             QMap<ObjectId, Message*> &leftMes);

    static void countRightSDI(const ObjectId &nextTask, const QList<Message*> &messages, const QMap <ObjectId, double> &messageDur,
                      QMap <ObjectId, QList<double>> &rightSDI, const QMap <ObjectId, CoreId> taskCore, const TimeType T, ScheduleData * data,
                              QMap<ObjectId, Message*> &rightMes);
};
}
#endif // SDICOUNTING

