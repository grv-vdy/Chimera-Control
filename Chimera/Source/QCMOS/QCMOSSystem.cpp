#include "stdafx.h"
#include "QCMOSSystem.h"

QCMOSSystem::QCMOSSystem() {
    corePtr = std::make_unique<QCMOSCore>();
}

QCMOSCore& QCMOSSystem::core() {
    return *corePtr;
}

const QCMOSCore& QCMOSSystem::core() const {
    return *corePtr;
}

QCMOSFlume& QCMOSSystem::flume() {
    return corePtr->getFlume();
}

const QCMOSFlume& QCMOSSystem::flume() const {
    return corePtr->getFlume();
}
