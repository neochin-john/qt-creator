// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QStandardItemModel>

namespace EffectComposer {

class Uniform;

class EffectComposerUniformsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DisplayNameRole,
        DescriptionRole,
        ValueRole,
        BackendValueRole,
        DefaultValueRole,
        MaxValueRole,
        MinValueRole,
        TypeRole,
        ControlTypeRole,
        UseCustomValueRole,
        UserAdded,
        IsInUse
    };

    EffectComposerUniformsModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Q_INVOKABLE bool resetData(int row);
    Q_INVOKABLE bool remove(int row);
    Q_INVOKABLE QStringList displayNames() const;
    Q_INVOKABLE QStringList uniformNames() const;

    void resetModel();

    void addUniform(Uniform *uniform);
    void updateUniform(int uniformIndex, Uniform *uniform);

    QList<Uniform *> uniforms() const;

signals:
    void uniformRenamed(QString oldName, QString newName);

private:
    QList<Uniform *> m_uniforms;
};

} // namespace EffectComposer

