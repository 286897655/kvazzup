#include "sipfieldcomposing.h"

#include "sipconversions.h"

#include <QDebug>


// a function used within this file to add a parameter
bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue);


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding)
{
  if(request.requestURI.host == "")
  {
    qWarning() << "ERROR: Request URI host is empty when compising first line:"
               << request.requestURI.host;
  }

  if(request.type == SIP_NO_REQUEST)
  {
    qDebug() << "WARNING: First request line failed";
    return false;
  }
  if(request.type != SIP_REGISTER)
  {
    line = requestToString(request.type) + " sip:"
        + request.message->to.username + "@" + request.message->to.host
        + " SIP/" + request.message->version + lineEnding;
  }
  else
  {

    line = requestToString(request.type) + " sips:"
        + request.requestURI.host
        + " SIP/" + request.message->version + lineEnding;
  }

  return true;
}

bool getFirstResponseLine(QString& line, SIPResponse& response, QString lineEnding)
{
  if(response.type == SIP_UNKNOWN_RESPONSE)
  {
    qDebug() << "WARNING: First response line failed";
    return false;
  }
  line = "SIP/" + response.message->version + " "
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type) + lineEnding;
  return true;
}

bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->to.username != "" && message->to.host != "");
  if(message->to.username == "" ||  message->to.host == "")
  {
    qDebug() << "WARNING: Composing To-field failed because host is:"
             << message->to.host << "and" << message->to.username;
    return false;
  }

  SIPField field;
  field.name = "To";
  if(message->to.realname != "")
  {
    field.values = message->to.realname + " ";
  }
  field.values += "<sip:" + message->to.username + "@" + message->to.host + ">";
  field.parameters = nullptr;

  tryAddParameter(field, "tag", message->dialog->toTag);

  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->from.username != "" && message->from.host != "");
  if(message->from.username == "" ||  message->from.host == "")
  {
    qDebug() << "WARNING: From field failed";
    return false;
  }

  SIPField field;
  field.name = "From";
  if(message->from.realname != "")
  {
    field.values = message->from.realname + " ";
  }
  field.values += "<sip:" + message->from.username + "@" + message->from.host + ">";
  field.parameters = nullptr;

  tryAddParameter(field, "tag", message->dialog->fromTag);

  fields.push_back(field);
  return true;
}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->cSeq != 0 && message->transactionRequest != SIP_NO_REQUEST);
  if(message->cSeq == 0 || message->transactionRequest == SIP_NO_REQUEST)
  {
    qDebug() << "WARNING: cSeq field failed";
    return false;
  }

  SIPField field;
  field.name = "CSeq";

  field.values = QString::number(message->cSeq) + " " + requestToString(message->transactionRequest);
  field.parameters = nullptr;

  fields.push_back(field);
  return true;
}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->dialog->callID != "");
  if(message->dialog->callID == "")
  {
    qDebug() << "WARNING: Call-ID field failed";
    return false;
  }

  fields.push_back({"Call-ID", message->dialog->callID, nullptr});
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(!message->senderReplyAddress.empty());
  if(message->senderReplyAddress.empty())
  {
    qDebug() << "WARNING: Via field failed";
    return false;
  }

  for(ViaInfo via : message->senderReplyAddress)
  {
    Q_ASSERT(via.type != ANY);
    Q_ASSERT(via.branch != "");
    Q_ASSERT(via.address != "");

    SIPField field;
    field.name = "Via";

    field.values = "SIP/" + via.version +"/" + connectionToString(via.type) + " " + via.address;

    if(!tryAddParameter(field, "branch", via.branch))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }
    fields.push_back(field);
  }
  return true;
}

bool includeMaxForwardsField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->maxForwards != 0);
  if(message->maxForwards == 0)
  {
    qDebug() << "WARNING: Max-forwards field failed";
    return false;
  }

  fields.push_back({"Max-Forwards", QString::number(message->maxForwards), nullptr});
  return true;
}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->contact.username != "" && message->contact.host != "");
  if(message->contact.username == "" ||  message->contact.host == "")
  {
    qDebug() << "WARNING: Contact field failed";
    return false;
  }

  SIPField field;
  field.name = "Contact";
  field.values = "<sip:" + message->contact.username + "@" + message->contact.host + ">";
  field.parameters = nullptr;
  fields.push_back(field);
  return true;
}

bool includeContentTypeField(QList<SIPField> &fields,
                             QString contentType)
{
  Q_ASSERT(contentType != "");
  if(contentType == "")
  {
    qDebug() << "WARNING: Content-type field failed";
    return false;
  }
  fields.push_back({"Content-Type", contentType, nullptr});
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  fields.push_back({"Content-Length", QString::number(contentLenght), nullptr});
  return true;
}

bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue)
{
  if(parameterValue == "")
  {
    return false;
  }

  if(field.parameters == nullptr)
  {
    field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  field.parameters->append({parameterName, parameterValue});

  return true;
}
