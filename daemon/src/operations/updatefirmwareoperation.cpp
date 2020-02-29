#include "updatefirmwareoperation.h"
#include "bipfirmwareservice.h"
#include "typeconversion.h"
#include <QApplication>

UpdateFirmwareOperation::UpdateFirmwareOperation(const AbstractFirmwareInfo *info, QBLEService *service) : AbstractOperation(service), m_info(info)
{
}

void UpdateFirmwareOperation::start()
{
    if (m_info->type() != AbstractFirmwareInfo::Invalid) {
        BipFirmwareService *serv = dynamic_cast<BipFirmwareService*>(m_service);

        m_service->enableNotification(serv->UUID_CHARACTERISTIC_FIRMWARE);

        if (!sendFwInfo()) {
            m_service->message("Error sending firmware info, aborting.");
            //done();
        }
    } else {
        m_service->message(QObject::tr("File does not seem to be supported"));
    }
}

bool UpdateFirmwareOperation::handleMetaData(const QByteArray &value)
{
    qDebug() << "UpdateFirmwareOperation::handleMetaData:" << value;

    if (value.length() != 3) {
        qDebug() << "Notifications should be 3 bytes long.";
        return true;
    }
    bool success = value[2] == BipFirmwareService::SUCCESS;

    if (value[0] == BipFirmwareService::RESPONSE && success) {
        switch (value[1]) {
        case BipFirmwareService::COMMAND_FIRMWARE_INIT: {
            sendFirmwareData();
            break;
        }
        case BipFirmwareService::COMMAND_FIRMWARE_START_DATA: {
            sendChecksum();
            break;
        }
        case BipFirmwareService::COMMAND_FIRMWARE_CHECKSUM: {
            if (m_info->type() == AbstractFirmwareInfo::Firmware) {
                m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, QByteArray(1, BipFirmwareService::COMMAND_FIRMWARE_REBOOT));
            } else {
                m_service->message(QObject::tr("Update operation complete"));
                return true;
            }
            break;
        }
        case BipFirmwareService::COMMAND_FIRMWARE_REBOOT: {
            qDebug() << "Reboot command successfully sent.";

            return true;
            break;
        }
        default: {
            qDebug() << "Unexpected response during firmware update: ";
            //operationFailed();
            m_service->message(QObject::tr("Update operation failed"));
            return true;
        }
        }
        return false;
    }
    
    qDebug() << "Unexpected notification during firmware update: ";
    m_service->message(QObject::tr("Update operation failed, unexpected metadata"));
    return true;

}

void UpdateFirmwareOperation::handleData(const QByteArray &data)
{
    
}

bool UpdateFirmwareOperation::sendFwInfo()
{
    int fwSize = m_fwBytes.size();
    QByteArray sizeBytes = TypeConversion::fromInt24(fwSize);
    int arraySize = 10;
    //bool isFirmwareCode = m_info->type() == AbstractFirmwareInfo::Firmware;
    //if (!isFirmwareCode) {
    //    arraySize++;
    //}
    QByteArray bytes(arraySize, 0x00);
    int i = 0;
    bytes[i++] = BipFirmwareService::COMMAND_FIRMWARE_INIT;
    bytes[i++] = m_info->type();
    bytes[i++] = sizeBytes[0];
    bytes[i++] = sizeBytes[1];
    bytes[i++] = sizeBytes[2];
    bytes[i++] = 0; // TODO: what is that?
    //int crc32 = firmwareInfo.getCrc32();
    //byte[] crcBytes = BLETypeConversions.fromUint32(crc32);
    //bytes[i++] = crcBytes[0];
    //bytes[i++] = crcBytes[1];
    //bytes[i++] = crcBytes[2];
    //bytes[i] = crcBytes[3];

    m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, bytes);
    return true;
}

void UpdateFirmwareOperation::sendFirmwareData() 
{
    int len = m_fwBytes.length();
    int packetLength = 20;
    int packets = len / packetLength;

    // going from 0 to len
    int firmwareProgress = 0;
    int progressPercent = 0;

    BipFirmwareService *serv = dynamic_cast<BipFirmwareService*>(m_service);


//    if (prefs.getBoolean("mi_low_latency_fw_update", true)) {
//        getSupport().setLowLatency(builder);
//    }
    m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, QByteArray(1, BipFirmwareService::COMMAND_FIRMWARE_START_DATA));

    for (int i = 0; i < packets; i++) {
        QByteArray fwChunk = m_fwBytes.mid(i * packetLength, packetLength);

        m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE_DATA, fwChunk);
        firmwareProgress += packetLength;

        progressPercent = (int) ((((float) firmwareProgress) / len) * 100);
        if ((i > 0) && (i % 100 == 0)) {
            m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, QByteArray(1, BipFirmwareService::COMMAND_FIRMWARE_UPDATE_SYNC));
            serv->downloadProgress(progressPercent);
            QApplication::processEvents();
        }
    }

    if (firmwareProgress < len) {
        QByteArray lastChunk = m_fwBytes.mid(packets * packetLength);
        m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE_DATA, lastChunk);
        firmwareProgress = len;
    }

    serv->downloadProgress(100);

    m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, QByteArray(1, BipFirmwareService::COMMAND_FIRMWARE_UPDATE_SYNC));
}


void UpdateFirmwareOperation::sendChecksum() 
{
    int crc16 = m_info->crc16();
    m_service->writeValue(BipFirmwareService::UUID_CHARACTERISTIC_FIRMWARE, QByteArray(1, BipFirmwareService::COMMAND_FIRMWARE_CHECKSUM) + TypeConversion::fromInt16(crc16));
}

QString UpdateFirmwareOperation::version()
{
    return m_info->version();
}
