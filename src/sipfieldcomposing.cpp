#include "sipfieldcomposing.h"

#include "sipconversions.h"

#include <QDebug>


// a function used within this file to add a parameter
bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue);


bool getFirstRequestLine(QString& line, SIPRequest& request)
{
  if(request.type == SIP_UNKNOWN_REQUEST)
  {
    return false;
  }
  line = requestToString(request.type) + " "
      + request.message->routing->to.username + "@" + request.message->routing->to.host
      + " SIP/" + request.message->version;
  return true;
}

bool getFirstResponseLine(QString& line, SIPResponse& response)
{
  if(response.type == SIP_UNKNOWN_RESPONSE)
  {
    return false;
  }
  line = " SIP/" + response.message->version
      + QString::number(responseToCode(response.type)) + " "
      + responseToPhrase(response.type);
  return true;
}

bool includeToField(QList<SIPField> &fields,
                    std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->to.username != "" && message->routing->to.host != "");
  if(message->routing->to.username == "" ||  message->routing->to.host == "")
  {
    return false;
  }

  SIPField field;
  field.name = "To";
  if(message->routing->to.realname != "")
  {
    field.values = message->routing->to.realname + " ";
  }
  field.values += "<" + message->routing->to.username + "@" + message->routing->to.host + ">";
  field.parameters = NULL;

  tryAddParameter(field, "tag", message->session->toTag);

  fields.push_back(field);
  return true;
}

bool includeFromField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->from.username != "" && message->routing->from.host != "");
  if(message->routing->from.username == "" ||  message->routing->from.host == "")
  {
    return false;
  }

  SIPField field;
  field.name = "From";
  if(message->routing->from.realname != "")
  {
    field.values = message->routing->from.realname + " ";
  }
  field.values += "<" + message->routing->from.username + "@" + message->routing->from.host + ">";
  field.parameters = NULL;

  tryAddParameter(field, "tag", message->session->fromTag);

  fields.push_back(field);
  return true;
}

bool includeCSeqField(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->cSeq != 0 && message->transactionRequest != SIP_UNKNOWN_REQUEST);
  if(message->cSeq == 0 || message->transactionRequest != SIP_UNKNOWN_REQUEST)
  {
    return false;
  }

  SIPField field;
  field.name = "CSeq";

  field.values = QString::number(message->cSeq) + " " + requestToString(message->transactionRequest);
  field.parameters = NULL;

  fields.push_back(field);
  return true;
}

bool includeCallIDField(QList<SIPField> &fields,
                        std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->session->callID != "");
  if(message->session->callID == "")
  {
    return false;
  }

  fields.push_back({"Call-ID", message->session->callID, NULL});
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(!message->routing->senderReplyAddress.empty());
  if(message->routing->senderReplyAddress.empty())
  {
    return false;
  }

  for(ViaInfo via : message->routing->senderReplyAddress)
  {
    Q_ASSERT(via.type != ANY);
    Q_ASSERT(via.branch != "");

    SIPField field;
    field.name = "Via";

    field.values = "SIP/" + via.version +"/" + connectionToString(via.type) + " " + via.address;

    if(!tryAddParameter(field, "branch", via.branch))
    {
      return false;
    }
  }
  return true;
}

bool includeMaxForwardsField(QList<SIPField> &fields,
                             std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->maxForwards != 0);
  if(message->routing->maxForwards == 0)
  {
    return false;
  }

  fields.push_back({"Max-Forwards", QString::number(message->routing->maxForwards), NULL});
  return true;
}

bool includeContactField(QList<SIPField> &fields,
                         std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(message->routing->contact.username != "" && message->routing->contact.host != "");
  if(message->routing->contact.username == "" ||  message->routing->contact.host == "")
  {
    return false;
  }

  SIPField field;
  field.name = "Contact";
  field.values = "<" + message->routing->contact.username + "@" + message->routing->contact.host + ">";
  field.parameters = NULL;
  fields.push_back(field);
  return true;
}

bool includeContentTypeField(QList<SIPField> &fields,
                             QString contentType)
{
  Q_ASSERT(contentType != "");
  if(contentType == "")
  {
    return false;
  }
  fields.push_back({"Content-Type", contentType, NULL});
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  fields.push_back({"Content-Length", QString::number(contentLenght), NULL});
  return true;
}

bool tryAddParameter(SIPField& field, QString parameterName, QString parameterValue)
{
  if(parameterValue == "")
  {
    return false;
  }

  if(field.parameters == NULL)
  {
    field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  field.parameters->append({parameterName, parameterValue});

  return true;
}
