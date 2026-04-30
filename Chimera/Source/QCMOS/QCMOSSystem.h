#pragma once

#include <memory>

#include "QCMOSCore.h"
#include "QCMOSFlume.h"

class QCMOSSystem {
public:
    QCMOSSystem();
    QCMOSSystem(const QCMOSSystem&) = delete;
    QCMOSSystem& operator=(const QCMOSSystem&) = delete;

    QCMOSCore& core();
    const QCMOSCore& core() const;

    QCMOSFlume& flume();
    const QCMOSFlume& flume() const;

private:
    std::unique_ptr<QCMOSCore> corePtr;
};
