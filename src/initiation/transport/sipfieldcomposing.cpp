#include "sipfieldcomposing.h"

#include "sipconversions.h"

#include "common.h"

#include <QDebug>


// a function used within this file to add a parameter
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName, QString parameterValue);
bool tryAddParameter(std::shared_ptr<QList<SIPParameter> > &parameters,
                     QString parameterName);

QString composeUritype(ConnectionType type);

QString composeUritype(ConnectionType type)
{
  if (type == TCP)
  {
    return "sip:";
  }
  else if (type == TLS)
  {
    return "sips:";
  }
  else if (type == TEL)
  {
    return "tel:";
  }
  else
  {
    qDebug() << "ERROR:  Unset connection type detected while composing SIP message";
  }

  return "";
}

QString composePortString(uint16_t port)
{
  QString portString = "";

  if (port != 0)
  {
    portString = ":" + QString::number(port);
  }
  return portString;
}

QString composeSIPUri(SIP_URI& uri)
{
  QString message = composeUritype(uri.connectionType);
  if (message != "")
  {
    message += uri.username + "@" + uri.host + composePortString(uri.port);
  }
  return message;
}


bool getFirstRequestLine(QString& line, SIPRequest& request, QString lineEnding)
{
  if(request.requestURI.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPComposing", DC_SEND_SIP_REQUEST,
               "Request URI host is empty when comprising the first line.");
  }

  if(request.type == SIP_NO_REQUEST)
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPComposing",
               DC_SEND_SIP_REQUEST, "SIP_NO_REQUEST given.");
    return false;
  }

  QString type = "";
  QString target = "";

  if(request.type != SIP_REGISTER)
  {
    type = composeUritype(request.message->to.connectionType);
    target = request.message->to.username + "@" + request.message->to.host;
  }
  else // REGISTER first line does not contain username.
  {
    type = composeUritype(request.requestURI.connectionType);
    target = request.requestURI.host;
  }

  line = requestToString(request.type) + " " + type
      + target + " SIP/" + request.message->version + lineEnding;

  return true;
}

bool getFirstResponseLine(QString& line, SIPResponse& response,
                          QString lineEnding)
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

  SIPField field = {"To", QList<ValueSet>{ValueSet{"", nullptr}}};

  if(message->to.realname != "")
  {
    field.valueSets[0].values = message->to.realname + " ";
  }

  QString uri = composeSIPUri(message->to);
  if (uri == "")
  {
    return false;
  }

  field.valueSets[0].values += "<" + uri + ">";
  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->dialog->toTag);

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

  SIPField field = {"From", QList<ValueSet>{ValueSet{"", nullptr}}};

  if(message->from.realname != "")
  {
    field.valueSets[0].values = message->from.realname + " ";
  }

  QString uri = composeSIPUri(message->from);

  if (uri == "")
  {
    return false;
  }

  field.valueSets[0].values += "<" + uri + ">";
  field.valueSets[0].parameters = nullptr;

  tryAddParameter(field.valueSets[0].parameters, "tag", message->dialog->fromTag);

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

  SIPField field = {"CSeq", QList<ValueSet>{ValueSet{"", nullptr}}};

  field.valueSets[0].values = QString::number(message->cSeq) + " " + requestToString(message->transactionRequest);
  field.valueSets[0].parameters = nullptr;

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

  SIPField field = {"Call-ID", QList<ValueSet>{ValueSet{message->dialog->callID, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeViaFields(QList<SIPField> &fields,
                      std::shared_ptr<SIPMessageInfo> message)
{
  Q_ASSERT(!message->vias.empty());
  if(message->vias.empty())
  {
    qDebug() << "WARNING: Via field failed";
    return false;
  }

  for(ViaInfo via : message->vias)
  {
    Q_ASSERT(via.connectionType != NONE);
    Q_ASSERT(via.branch != "");
    Q_ASSERT(via.address != "");

    SIPField field = {"Via", QList<ValueSet>{ValueSet{"", nullptr}}};

    field.valueSets[0].values = "SIP/" + via.version +"/" + connectionToString(via.connectionType)
        + " " + via.address + composePortString(via.port); //";alias;rport";

    if(!tryAddParameter(field.valueSets[0].parameters, "branch", via.branch))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    if(via.alias && !tryAddParameter(field.valueSets[0].parameters, "alias"))
    {
      qDebug() << "WARNING: Via field failed";
      return false;
    }

    if(via.rport && !tryAddParameter(field.valueSets[0].parameters, "rport"))
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

  SIPField field = {"Max-Forwards",
                    QList<ValueSet>{ValueSet{QString::number(message->maxForwards), nullptr}}};
  fields.push_back(field);
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

  SIPField field = {"Contact", QList<ValueSet>{ValueSet{"", nullptr}}};

  QString portString = "";

  if (message->contact.port != 0)
  {
    portString = ":" + QString::number(message->contact.port);
  }

  QString transportString = "";

  if (message->contact.connectionType == TCP)
  {
    transportString = ";transport=tcp";
  }

  field.valueSets[0].values = "<sip:" + message->contact.username + "@"
      + message->contact.host + portString + transportString + ">";

  field.valueSets[0].parameters = nullptr;

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
  SIPField field = {"Content-Type",
                    QList<ValueSet>{ValueSet{contentType, nullptr}}};
  fields.push_back(field);
  return true;
}

bool includeContentLengthField(QList<SIPField> &fields,
                               uint32_t contentLenght)
{
  SIPField field = {"Content-Length",
                    QList<ValueSet>{ValueSet{QString::number(contentLenght), nullptr}}};
  fields.push_back(field);
  return true;
}


bool includeExpiresField(QList<SIPField>& fields,
                         uint32_t expires)
{
  SIPField field = {"Expires",
                    QList<ValueSet>{ValueSet{QString::number(expires), nullptr}}};
  fields.push_back(field);
  return true;
}


bool tryAddParameter(std::shared_ptr<QList<SIPParameter>>& parameters,
                     QString parameterName, QString parameterValue)
{
  if (parameterValue == "" || parameterName == "")
  {
    return false;
  }

  if (parameters == nullptr)
  {
    parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  parameters->append({parameterName, parameterValue});

  return true;
}


bool tryAddParameter(std::shared_ptr<QList<SIPParameter>>& parameters,
                     QString parameterName)
{
  if (parameterName == "")
  {
    return false;
  }

  if (parameters == nullptr)
  {
    parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
  }
  parameters->append({parameterName, ""});

  return true;
}
