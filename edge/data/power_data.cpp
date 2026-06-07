#include "power_data.h"

PowerData::PowerData(time_t timestamp, double avg)
{
  this->timestamp = timestamp;
  this->avg = avg;
  this->next = NULL;
  this->unit = "kWh";
}

void PowerData::setNext(PowerData *next)
{
  this->next = next;
}

PowerData *PowerData::getNext()
{
  return this->next;
}