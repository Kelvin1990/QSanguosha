/********************************************************************
    Copyright (c) 2013-2014 - QSanguosha-Rara

    This file is part of QSanguosha-Hegemony.

    This game is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3.0
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    See the LICENSE file for more details.

    QSanguosha-Rara
    *********************************************************************/

#include "lobbyscene.h"
#include "client.h"
#include "settings.h"
#include "skinbank.h"
#include "clientstruct.h"
#include "tile.h"
#include "engine.h"

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>

int LobbyScene::SCENE_PADDING = 20;
int LobbyScene::SCENE_MARGIN_TOP = 30;


LobbyScene::LobbyScene(QMainWindow *parent) :
    QGraphicsScene(parent), currentPage(0), client(ClientInstance)
{
    setItemIndexMethod(NoIndex);

    //chat
    chatBox = new QTextEdit;
    chatBox->setObjectName("chat_log");
    chatBox->setReadOnly(true);
    chatLineEdit = new QLineEdit;
    chatLineEdit->setObjectName("chat_input");
    QVBoxLayout *chatLayout = new QVBoxLayout;
    chatLayout->addWidget(chatBox);
    chatLayout->addWidget(chatLineEdit);
    chatWidget = new QWidget;
    chatWidget->setObjectName("lobby_chat_box");
    chatWidget->setLayout(chatLayout);
    addWidget(chatWidget);

    connect(client, &Client::lineSpoken, chatBox, &QTextEdit::append);
    connect(chatLineEdit, &QLineEdit::editingFinished, this, &LobbyScene::speakToServer);

    //user profile
    userAvatarItem = addPixmap(G_ROOM_SKIN.getGeneralPixmap(Config.UserAvatar, QSanRoomSkin::S_GENERAL_ICON_SIZE_TINY));
    userNameItem = new Title(NULL, Config.UserName, "wqy-microhei", 16);
    addItem(userNameItem);

    //buttons
    refreshButton = new QPushButton(tr("Refresh"));
    exitButton = new QPushButton(tr("Exit"));
    QVBoxLayout *buttonLayout = new QVBoxLayout;
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(exitButton);
    buttonBox = new QWidget;
    buttonBox->setObjectName("lobby_button_box");
    buttonBox->setLayout(buttonLayout);
    addWidget(buttonBox);

    connect(refreshButton, &QPushButton::clicked, this, &LobbyScene::refreshRoomList);
    connect(exitButton, &QPushButton::clicked, this, &LobbyScene::exit);

    //room tiles
    roomTitle = new Title(NULL, tr("Rooms"), "wqy-microhei", 30);
    addItem(roomTitle);
    roomTitle->setPos(SCENE_PADDING * 2, SCENE_PADDING * 2);

    createRoomTile = new Tile(tr("Create Room"), QSizeF(200.0, 100.0));
    createRoomTile->setObjectName("create_room_button");
    connect(createRoomTile, &Tile::clicked, this, &LobbyScene::onCreateRoomClicked);
    addItem(createRoomTile);

    setBackgroundBrush(QPixmap(Config.BackgroundImage));

    connect(client, &Client::roomListChanged, this, &LobbyScene::setRoomList);
    connect(client, &Client::destroyed, this, &LobbyScene::onClientDestroyed);
    connect(client, &Client::roomChanged, this, &LobbyScene::onRoomChanged);
    connect(this, &LobbyScene::sceneRectChanged, this, &LobbyScene::onSceneRectChanged);
    connect(this, &LobbyScene::roomListRequested, client, &Client::fetchRoomList);
    connect(this, (void (LobbyScene::*)(qlonglong))(&LobbyScene::roomSelected), client, &Client::onPlayerChooseRoom);
}

LobbyScene::~LobbyScene()
{
    if (client && ClientInstance != client)
        client->deleteLater();
}

void LobbyScene::onSceneRectChanged(const QRectF &rect)
{
    chatWidget->resize(rect.width() - 250, rect.height() * 0.3);
    chatWidget->move(SCENE_PADDING, rect.height() - chatWidget->height() - SCENE_PADDING);

    userAvatarItem->setPos(rect.right() - userAvatarItem->boundingRect().width() - SCENE_PADDING, SCENE_MARGIN_TOP + SCENE_PADDING);
    userNameItem->setPos(userAvatarItem->x(), userAvatarItem->y() + userAvatarItem->boundingRect().height());

    buttonBox->resize(200.0, rect.height() * 0.3);
    buttonBox->move(rect.width() - buttonBox->width() - SCENE_PADDING, rect.height() - buttonBox->height() - SCENE_PADDING);

    adjustRoomTiles();
}

void LobbyScene::adjustRoomTiles()
{
    static const int SPACING = 15;
    QRectF displayRegion = sceneRect();
    displayRegion.setWidth(displayRegion.width() * 0.82);
    displayRegion.setHeight(displayRegion.height() * 0.6 - roomTitle->boundingRect().height());
    displayRegion.setX(SCENE_PADDING * 2);
    displayRegion.setY(SCENE_PADDING * 2 + roomTitle->boundingRect().height() + 10);

    int x = displayRegion.x(), y = displayRegion.y();
    int roomNum = roomTiles.size();
    int i;
    for (i = 0; i < roomNum; i++) {
        Tile *tile = roomTiles.at(i);

        tile->setPos(x, y);
        tile->show();

        x += tile->boundingRect().width() + SPACING;
        if (x + tile->boundingRect().width() > displayRegion.right()) {
            x = displayRegion.x();
            y += tile->boundingRect().height() + SPACING;
            if (y + tile->boundingRect().height() > displayRegion.bottom()) {
                break;
            }
        }
    }
    int final = i;
    for(; i < roomNum; i++) {
        roomTiles.at(i)->hide();
    }

    if (y + createRoomTile->boundingRect().height() > displayRegion.bottom()) {
        roomTiles.at(final)->hide();
        x = roomTiles.at(final)->x();
        y = roomTiles.at(final)->y();
    }
    createRoomTile->setPos(x, y);
}

void LobbyScene::setRoomList(const QList<RoomInfoStruct> *data)
{
    rooms = data;

    foreach (Tile *tile, roomTiles) {
        removeItem(tile);
        tile->deleteLater();
    }
    roomTiles.clear();

    foreach (const RoomInfoStruct &info, *data) {
        Tile *tile = new Tile(info.Name, QSizeF(200.0, 100.0));
        tile->setAutoHideTitle(false);
        QStringList scrollTexts;

        if (info.PlayerNum >= 0)
            scrollTexts << tr("Player Number: %1 / %2").arg(info.PlayerNum).arg(Sanguosha->getPlayerCount(info.GameMode));
        else
            scrollTexts << tr("Player Number: %1 / %2").arg('-').arg(Sanguosha->getPlayerCount(info.GameMode));

        scrollTexts << (info.OperationTimeout == 0 ?
            tr("There is no time limit") :
            tr("Operation timeout is %1 seconds").arg(info.OperationTimeout));

        if (info.EnableCheat) {
            scrollTexts << tr("Cheat is enabled");
            scrollTexts << (info.FreeChoose ? tr("Free choose is enabled") : tr("Free choose is disabled"));
        } else {
            scrollTexts << tr("Cheat is disabled");
        }

        if (info.FirstShowingReward)
            scrollTexts << tr("The reward of showing general first is enabled");
        if (info.ForbidAddingRobot)
            scrollTexts << tr("AI is disabled");
        else
            scrollTexts << tr("AI is enabled");

        tile->addScrollTexts(scrollTexts);

        roomTiles << tile;
        addItem(tile);
        connect(tile, &Tile::clicked, this, &LobbyScene::onRoomTileClicked);
    }

    adjustRoomTiles();
}

void LobbyScene::speakToServer()
{
    if (client == NULL)
        return;

    QString message = chatLineEdit->text();
    if (!message.isEmpty()) {
        client->speakToServer(message);
        chatLineEdit->clear();
    }
}

void LobbyScene::refreshRoomList()
{
    currentPage = 0;
    emit roomListRequested(currentPage);
}

void LobbyScene::prevPage()
{
    if (currentPage > 0)
        emit roomListRequested(--currentPage);
}

void LobbyScene::nextPage()
{
    if (!rooms->isEmpty())
        emit roomListRequested(++currentPage);
}

void LobbyScene::onRoomTileClicked()
{
    Tile *tile = qobject_cast<Tile *>(sender());
    if (tile == NULL) return;

    int index = roomTiles.indexOf(tile);
    if (index == -1 || index >= rooms->size()) return;

    const RoomInfoStruct &info = rooms->at(index);

    if (info.RequirePassword) {
        QString password = QInputDialog::getText(NULL, tr("Please input the password"), tr("Password:"), QLineEdit::Password);
        if (password.isEmpty())
            return;
        else
            Config.RoomPassword = password;
    }

    if (info.HostAddress.isEmpty()) {
        emit roomSelected(info.RoomId);
    } else {
        Config.HostAddress = info.HostAddress;
        emit roomSelected();
    }
}

void LobbyScene::onCreateRoomClicked()
{
    Config.LobbyAddress = Config.HostAddress;
    emit createRoomClicked();
}

void LobbyScene::onClientDestroyed()
{
    client = NULL;
    disconnect(chatLineEdit, &QLineEdit::editingFinished, this, &LobbyScene::speakToServer);
}

void LobbyScene::onRoomChanged(int index)
{
    const RoomInfoStruct &info = rooms->at(index);
    Tile *tile = roomTiles.at(index);
    if (tile)
        tile->setScrollText(0, tr("Player Number: %1 / %2").arg(info.PlayerNum).arg(Sanguosha->getPlayerCount(info.GameMode)));
}
