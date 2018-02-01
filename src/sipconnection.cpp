#include "sipconnection.h"

#include "sipconversions.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QList>
#include <QHostInfo>

#include <iostream>
#include <sstream>
#include <string>


enum FieldPrecense {MANDATORY, SHOULD, COPIED, OPTIONAL, ABSENT};

struct HeaderInfo
{
  QString condition;

  FieldPrecense ACK;
  FieldPrecense BYE;
  FieldPrecense CANCEL;
  FieldPrecense INVITE;
  FieldPrecense OPTIONS;
  FieldPrecense REGISTER;
};

SIPConnection::SIPConnection(quint32 sessionID):
  partialMessage_(""),
  connection_(sessionID, false),
  sessionID_(sessionID)
{}

SIPConnection::~SIPConnection()
{}
/*
const std::map<QString, uint16_t> fieldMinValues = {{"Call-ID",1},
                                                   {"Contact",1},
                                                   {"To", }};
*/
void SIPConnection::initConnection(ConnectionType type, QHostAddress target)
{
  qWarning() << "WARNING: SIPConnection not implemented yet.";


}

void SIPConnection::sendRequest(RequestType request,
                 std::shared_ptr<SIPRoutingInfo> routing,
                 std::shared_ptr<SIPSessionInfo> session,
                 std::shared_ptr<SIPMessageInfo> message)
{
  qWarning() << "WARNING: SIPConnection not implemented yet.";
}

void SIPConnection::sendResponse(ResponseType response,
                  std::shared_ptr<SIPRoutingInfo> routing,
                  std::shared_ptr<SIPSessionInfo> session,
                  std::shared_ptr<SIPMessageInfo> message)
{
  qWarning() << "WARNING: SIPConnection not implemented yet.";
}

void SIPConnection::networkPackage(QString message)
{
  qDebug() << "Received a network package for SIP Connection";
  // parse to header and body
  QString header = "";
  QString body = "";
  parsePackage(message, header, body);

  std::shared_ptr<QList<SIPField>> fields;
  std::shared_ptr<QStringList> firstLine;

  if(header != "")
  {
    fields = SIPConnection::networkToFields(header, firstLine);
  }
  else
  {
    // parsing was not successfull. Possibly because the whole message was not received.
    return;
  }

  if(!checkFields(firstLine, fields))
  {
    qDebug() << "The received message was not correct. ";
    emit parsingError(SIP_BAD_REQUEST, sessionID_); // TODO support other possible error types
  }

  processFields(fields);
}

void SIPConnection::parsePackage(QString package, QString& header, QString& body)
{
  qDebug() << "Parsing package to header and body.";

  if(partialMessage_.length() > 0)
  {
    package = partialMessage_ + package;
    partialMessage_ = "";
  }

  int headerEndIndex = package.indexOf("\r\n\r\n", 0, Qt::CaseInsensitive) + 4;
  int contentLengthIndex = package.indexOf("content-length", 0, Qt::CaseInsensitive);

  qDebug() << "header end at:" << headerEndIndex
           << "and content-length at:" << contentLengthIndex;

  if(contentLengthIndex != -1 && headerEndIndex != -1)
  {
    int contentLengthLineEndIndex = package.indexOf("\r\n", contentLengthIndex, Qt::CaseInsensitive);

    QString value = package.mid(contentLengthIndex + 16, contentLengthLineEndIndex - (contentLengthIndex + 16));

    int valueInt= value.toInt();

    qDebug() << "Content-length:" <<  valueInt;

    if(package.length() < headerEndIndex + valueInt)
    {
      partialMessage_ = package;
    }
    else
    {
      partialMessage_ = package.right(package.length() - (headerEndIndex + valueInt));
      header = package.left(headerEndIndex);
      body = package.mid(headerEndIndex, valueInt);

      qDebug() << "Whole message received.";
      qDebug() << "Header:" << header;
      qDebug() << "Content:" << body;
      qDebug() << "Left overs:" << partialMessage_;
    }
  }
  else
  {
    qDebug() << "Message was not received fully";
    partialMessage_.append(package);
  }
}

std::shared_ptr<QList<SIPField>> SIPConnection::networkToFields(QString header, std::shared_ptr<QStringList>& firstLine)
{
  firstLine = std::shared_ptr<QStringList>(new QStringList);

  QStringList lines = header.split("\r\n", QString::SkipEmptyParts);
  if(lines.size() == 0)
  {
    return NULL;
  }
  *firstLine.get() = lines[0].split(" ", QString::SkipEmptyParts);

  std::shared_ptr<QList<SIPField>> fields = std::shared_ptr<QList<SIPField>>(new QList<SIPField>);
  for(unsigned int i = 1; i < lines.size(); ++i)
  {
    SIPField field = {"",NULL,NULL};
    // separate paramters from line using ; mark
    QStringList parameters = lines[i].split(";", QString::SkipEmptyParts);
    for(unsigned int j = 1; j < parameters.size(); ++j)
    {
      QStringList nameValue = lines[i].split("=", QString::SkipEmptyParts);
      if(nameValue.size() == 2)
      {
        SIPParameter parameter;
        parameter.name = nameValue[0];
        parameter.value = nameValue[1];
        if(field.parameters == NULL)
        {
          field.parameters = std::shared_ptr<QList<SIPParameter>> (new QList<SIPParameter>);
        }
        field.parameters->append(parameter);
      }
      else
      {
        qDebug() << "Can't understand parameter with more than on \"=\" mark";
      }
    }

    if(parameters.size() >= 1)
    {
      QStringList nameValues = parameters[0].split(":", QString::SkipEmptyParts);
      if(nameValues.size() >= 2)
      {
        field.name = nameValues[0];
        field.values = std::shared_ptr<QStringList> (new QStringList);
        for(unsigned int j = 1; j < nameValues.size(); ++j)
        {
          field.values->append(nameValues[j]);
        }
      }
      else
      {
        qDebug() << "Either field name or value missing in sip line:" << nameValues;
      }
    }
    fields->append(field);
  }

  qDebug() << "Fields parsed. Found" << fields->size() << "fields.";
  return fields;
}

bool SIPConnection::checkFields(std::shared_ptr<QStringList> firstLine, std::shared_ptr<QList<SIPField>> fields)
{
  qDebug() << "General checking of SIP message integrity";
  if(firstLine == NULL || fields == NULL)
  {
    qDebug() << "Did not find enough lines in SIP message.";
    return false;
  }

  // first we check that the first line is parsed correctly
  if(firstLine->size() < 3)
  {
    qDebug() << "Not enough words in first SIP message line:" << firstLine->size();
    return false;
  }
  qDebug() << "Checking only that absolutely mandatory header fields are present.";
  // check that all required header lines are present
  if(!linePresent("To", fields)
     || !linePresent("From", fields)
     || !linePresent("CSeq", fields)
     || (!linePresent("Call-ID", fields) && !linePresent("i", fields))
     | !linePresent("Via", fields))
  {
    qDebug() << "All mandatory header lines not present!";
    return false;
  }

  if(firstLine->at(2) == "SIP/2.0") // Request
  {
    RequestType request = stringToRequest(firstLine->at(0));

    if(request == SIP_UNKNOWN_REQUEST)
    {
      qDebug() << "Could not recognize request type!";
      return false;
    }

    if(!linePresent("Max-Forwards", fields))
    {
      qDebug() << "Mandatory Max-Forwards not present in Request header";
      return false;
    }
    qDebug() << "Identified a request:" << firstLine->at(0);
    if(request == INVITE && !linePresent("Contact", fields))
    {
      qDebug() << "Contact header missing from INVITE request";
      return false;
    }
  }
  else if(firstLine->at(0) == "SIP/2.0") // Response
  {
    uint16_t responseCode = stringResponseCode(firstLine->at(1));

    if(responseCode < 100 || responseCode > 607)
    {
      qDebug() << "Response code does not make sense:" << responseCode;
      return false;
    }
    qDebug() << "Identified a response:" << responseCode;
  }
  else // not recognized
  {
    qDebug() << "SIP message format not recognized!";
    return false;
  }

  for(SIPField field : *fields.get())
  {
    if(field.values == NULL || field.values->size() == 0)
    {
      qDebug() << "No value present for a field.";
      if(field.name == "To" ||
         field.name == "From" ||
         field.name == "CSeq" ||
         field.name == "Call-ID" ||
         field.name == "Via" ||
         field.name == "Max-Forwards")
      {
        qDebug() << "and that field is important:" << field.name;
        return false;
      }
    }
  }

  qDebug() << "Header checked and it is ok.";
  return true;
}

bool SIPConnection::linePresent(QString name, std::shared_ptr<QList<SIPField>> fields)
{
  for(SIPField field : *fields.get())
  {
    if(field.name == name)
    {
      return true;
    }
  }
  qDebug() << "Did not find header:" << name;
  return false;
}

void SIPConnection::processFields(std::shared_ptr<QList<SIPField>> fields)
{

}

std::shared_ptr<SDPMessageInfo> SIPConnection::parseSDPMessage(QString& body)
{
  std::shared_ptr<SDPMessageInfo> info(new SDPMessageInfo);
  bool version = false;
  bool originator = false;
  bool session = false;
  bool timing = false;
  bool globalContact = false;
  bool localContacts = true;

  QStringList lines = body.split("\r\n", QString::SkipEmptyParts);
  QStringListIterator lineIterator(lines);

  while(lineIterator.hasNext())
  {
    QString line = lineIterator.next();

    QStringList words = line.split(" ", QString::SkipEmptyParts);
    QString firstValue = "";

    switch(line.at(0).toLatin1())
    {
      case 'v':
      {

        if(!checkSDPLine(words, 1, firstValue))
          return NULL;

        info->version = firstValue.toUInt();
        if(info->version != 0)
        {
          qDebug() << "Unsupported SDP version:" << info->version;
          return NULL;
        }

        version = true;
        break;
      }
      case 'o':
      {
        if(!checkSDPLine(words, 6, info->originator_username))
          return NULL;

        info->sess_id = words.at(1).toUInt();
        info->sess_v = words.at(2).toUInt();
        info->host_nettype = words.at(3);
        info->host_addrtype = words.at(4);
        info->host_address = words.at(5);

        qDebug() << "Origin read successfully. host_address:" << info->host_address;

        originator = true;
        break;
      }
      case 's':
      {
        if(line.size() < 3 )
          return NULL;

        info->sessionName = line.right(line.size() - 2);

        qDebug() << "Session name:" << info->sessionName;
        session = true;
        break;
      }
      case 't':
      {
        if(!checkSDPLine(words, 2, firstValue))
          return NULL;

        info->startTime = firstValue.toUInt();
        info->endTime = words.at(1).toUInt();

        timing = true;
        break;
      }
      case 'm':
      {
        MediaInfo mediaInfo;
        if(!checkSDPLine(words, 4, mediaInfo.type))
          return NULL;

        bool mediaContact = false;
        mediaInfo.receivePort = words.at(1).toUInt();
        mediaInfo.proto = words.at(2);
        mediaInfo.rtpNum = words.at(3).toUInt();

        qDebug() << "Media found with type:" << mediaInfo.type;

        // TODO process other possible lines
        while(lineIterator.hasNext()) // TODO ERROR if not correct, the line is not processsed, move backwards?
        {
          QString additionalLine = lineIterator.next();
          QStringList additionalWords = additionalLine.split(" ", QString::SkipEmptyParts);


          if(additionalLine.at(0) == 'c')
          {
            if(!checkSDPLine(additionalWords, 3, mediaInfo.nettype))
              return NULL;

            mediaInfo.addrtype = additionalWords.at(1);
            mediaInfo.address = additionalWords.at(2);

            info->connection_addrtype = additionalWords.at(1); // TODO: what about nettype?
            info->connection_address = additionalWords.at(2);

            mediaContact = true;
          }
          else if(additionalLine.left(8) == "a=rtpmap")
          {
            if(additionalWords.size() >= 2)
            {
              if(additionalWords.at(0).size() < 3)
              {
                qDebug() << "rtpmap missing first value";
                break;
              }

              QStringList payloadNum = additionalWords.at(0).split(":", QString::SkipEmptyParts);
              QStringList encoding = additionalWords.at(1).split("/", QString::SkipEmptyParts);

              if(payloadNum.size() != 2 || encoding.size() != 2)
                break;

              RTPMap mapping;
              mapping.rtpNum = payloadNum.at(1).toUInt();
              mapping.codec = encoding.at(0);
              mapping.clockFrequency = encoding.at(1).toUInt();

              qDebug() << "RTPmap found with codec:" << mapping.codec;

              mediaInfo.codecs.append(mapping);
            }
            else
            {
              qDebug() << "too few words in rtp map";
              break;
            }

            if(additionalWords.size() < 4)
            {
              qDebug() << "Detected unsupported encoding parameter in:" << additionalWords;
              // TODO support encoding parameters
            }
          }
          else
          {
            lineIterator.previous();
            break;
          }
        }

        info->media.append(mediaInfo);

        if(!mediaContact)
          localContacts = false;

        break;
      }
      case 'c':
      {
        if(!checkSDPLine(words, 3, info->connection_nettype))
          return NULL;

        info->connection_addrtype = words.at(1);
        info->connection_address = words.at(2);
        globalContact = true;
        break;
      }
      default:
      {
        qDebug() << "Unimplemented or malformed SDP line type:" << line;
      }
    }
  }

  if(!version || !originator || !session || !timing || (!globalContact && !localContacts))
  {
    qDebug() << "All required fields not present in SDP:"
             << "v" << version << "o" << originator << "s" << session << "t" << timing
             << "global c" << globalContact << "local c" << localContacts;
    //return NULL;
  }

  return info;
}

void SIPConnection::parseSIPaddress(QString address, QString& user, QString& location)
{
  QStringList splitAddress = address.split("@");

  if(splitAddress.size() != 2)
  {
    user = "";
    location = "";
    return;
  }

  user = splitAddress.at(0).right(splitAddress.at(0).length() - 5);
  location = splitAddress.at(1).left(splitAddress.at(1).length() - 1);
}

void SIPConnection::parseSIPParameter(QString field, QString parameterName,
                       QString& parameterValue, QString& remaining)
{
  QStringList parameterSplit = field.split(parameterName);

  if(parameterSplit.size() != 2)
  {
    qDebug() << "Did not find" << parameterName << "in" << field;
    parameterValue = "";
  }
  else
  {
    qDebug() << "Found" << parameterName;
    parameterValue = parameterSplit.at(1);
  }
  remaining = parameterSplit.at(0);
}

QList<QHostAddress> SIPConnection::parseIPAddress(QString address)
{
  QList<QHostAddress> ipAddresses;

  QRegularExpression re("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  if(re.match(address).hasMatch())
  {
    qDebug() << "Found IPv4 address:" << address;
    ipAddresses.append(QHostAddress(address));
  }
  else
  {
    qDebug() << "Did not find IPv4 address:" << address;
    QHostInfo hostInfo;
    hostInfo.setHostName(address);

    ipAddresses.append(hostInfo.addresses());
  }
  return ipAddresses;
}

bool SIPConnection::checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue)
{
  Q_ASSERT(expectedLength != 0);

  if(line.size() != expectedLength)
  {
    qDebug() << "SDP line:" << line << "not expected length:" << expectedLength;
    return false;
  }
  if(line.at(0).length() < 3)
  {
    qDebug() << "First word missing in SDP Line:" << line;
    return false;
  }

  firstValue = line.at(0).right(line.at(0).size() - 2);

  return true;
}
