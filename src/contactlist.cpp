#include "contactlist.h"

#include "contactlistitem.h"

#include <QSettings>
#include <QDebug>

ContactList::ContactList()
{

}

void ContactList::initializeList(QListWidget* list, ParticipantInterface* interface)
{
  list_ = list;

  QSettings settings;

  int size = settings.beginReadArray("contacts");
  qDebug() << "Reading contact list with" << size << "contacts";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value("name").toString();
    QString username = settings.value("userName").toString();
    QString address = settings.value("ip").toString();

    addContactToList(interface, name, username, address);
  }
  settings.endArray();
}

void ContactList::addContact(ParticipantInterface* interface,
                             QString name, QString username, QString address)
{
  Q_ASSERT(!address.isEmpty());

  if(name == "")
    name = "Anonymous";
  if(username == "")
    username = "anonymous";
  if(address == "")
    return;

  int index = doesAddressExist(address);

  if(index != -1)
  {
    qDebug() << "User already exists at index:" << index;
    return;
  }

  qDebug() << "Adding contact. Name:" << name << "username:" << username
         << "address:" << address;

  addContactToList(interface, name, username, address);

  // not the fastest, just the simplest way to do this
  writeListToSettings();
}

void ContactList::writeListToSettings()
{
  qDebug() << "Writing contactList with" << items_.size() << "items to settings.";
  QSettings settings;

  settings.beginWriteArray("contacts");
  int index = 0;
  for(auto contact : items_)
  {
    settings.setArrayIndex(index);
    ++index;
    settings.setValue("Name", contact->getName());
    settings.setValue("username", contact->getUserName());
    settings.setValue("ip", contact->getAddress());
  }
  settings.endArray();
}

void ContactList::addContactToList(ParticipantInterface *interface,
                                   QString name, QString username, QString address)
{
  Q_ASSERT(list_);

  items_.push_back(new ContactListItem(name, username, address));
  items_.back()->init(interface);

  // set list_ as a parent
  QListWidgetItem* item = new QListWidgetItem(list_);
  item->setSizeHint(QSize(150, 50));
  list_->addItem(item);
  list_->setItemWidget(item, items_.back());
}

int ContactList::doesAddressExist(QString address)
{
  for(unsigned int i = 0; i < items_.size(); ++i)
  {
    if(items_.at(i)->getAddress() == address)
      return i;
  }
  return -1;
}
