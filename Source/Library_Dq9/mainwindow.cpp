#include "mainwindow.h"
//#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setFixedSize(QSize(1200,700));
    setWindowIcon(QIcon(":/Dq9.ico"));
    setWindowTitle("DQ9 Offline Map Tool");
    Init();
}

MainWindow::~MainWindow()
{
}

void MainWindow::Init()
{
    _reverseSeedTable.clear();
    TreasureMapDetailData();
    CreateReverseSeedTable();

    QMenu *menu;
    menu = menuBar()->addMenu("&Fichier");
    QList<QAction*> act = {new QAction("&Nouveau", this), new QAction("&Ouvrir", this), new QAction("&Sauvegarder", this)};
    menu->addActions(act);
    connect(act[0], SIGNAL(triggered()), this, SLOT(MapSelect()));
    connect(act[1], SIGNAL(triggered()), this, SLOT(OpenMap()));
    connect(act[2], SIGNAL(triggered()), this, SLOT(SaveMap()));
    act[2]->setEnabled(false);
    act[2]->setObjectName("Save");
    act.clear();

    //Menu Info
    menu = menuBar()->addMenu("&Info");
    act = {new QAction("&Monstres", this),new QAction("&Info Map", this)};
    menu->addActions(act);
    connect(act[0], SIGNAL(triggered()), this, SLOT(InitMonsterUi()));
    connect(act[1], SIGNAL(triggered()), this, SLOT(InitInfoMapUi()));
    act[0]->setObjectName("Monster");
    act[0]->setEnabled(false);
    act[1]->setObjectName("InfoMap");
    act[1]->setEnabled(false);
    act.clear();

    //Menu Utilitaire
    menu = menuBar()->addMenu("&Utilitaire");
    act = {new QAction("&Hoimi", this)};
    menu->addActions(act);
    connect(act[0], SIGNAL(triggered()), this, SLOT(InitHoimiUi()));
    act.clear();

    /*
    menu = menuBar()->addMenu("&Aide");
    act = {new QAction("&Language", this)};
    connect(act[0], SIGNAL(triggered()), this, SLOT(LangChange()));
    */

    QFile file(":/zsetr.bin");
    file.open(QIODevice::ReadOnly);
    Color = file.readAll();
    file.close();

    QSqlDatabase DB = QSqlDatabase::addDatabase("QSQLITE");
    DB.setDatabaseName(QDir::currentPath()+"/DataBase/DQ9Maps.db");

    if(!DB.open())
    {
        QMessageBox::information(this, "Erreur", "Erreur de connection à la base de donnée");
        //qDebug() << "Erreur connection";
        this->close();
    }

    // 0 = Id,1 = English , 2 = French, 3 = Japanese
    Difficulty = GetTable("DifficultyTypes",2);
    Dungeon = GetTable("DungeonTypes",2);
    Material = GetTable("MaterialTypes",2);
    TableR = GetTable("Items",2);
    Monster = GetTable("Monster",2);
}

void MainWindow::OpenMap()
{
    OpenUi = new QWidget(this,Qt::Popup|Qt::Dialog);
    OpenUi->setWindowModality(Qt::ApplicationModal);
    OpenUi->setFixedSize(QSize(400,400));
    OpenUi->move(100,100);
    OpenUi->setWindowTitle("Ouvrir");
    OpenUi->show();

    QListWidget* lw = new QListWidget(OpenUi);
    lw->setGeometry(20,20,360,280);
    lw->setObjectName("LW");
    connect(lw, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(MapSelected(QListWidgetItem*)));
    connect(lw, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(Validate(QListWidgetItem*)));
    lw->show();

    QList<QStringList> List = GetTable("SavedMaps");

    if (!List.isEmpty())
    {
        for (int i=0; i<List[0].size(); i++)
        {
            lw->addItem(FrenchName(List[0][i].toUShort(), List[1][i].toInt()));
        }
    }

    //Ok
    QPushButton* pb1 = new QPushButton(OpenUi);
    pb1->setGeometry(200,360,75,22);
    pb1->setEnabled(false);
    pb1->setText("Ok");
    pb1->setObjectName("Ok");
    connect(pb1, SIGNAL(clicked()), this, SLOT(Validate()));
    pb1->show();

    //Cancel
    QPushButton* pb2 = new QPushButton(OpenUi);
    pb2->setGeometry(300,360,75,22);
    pb2->setText("Annuler");
    connect(pb2, SIGNAL(clicked()), this, SLOT(CancelOpen()));
    pb2->show();

}

QString MainWindow::FrenchName(ushort MS, uchar MR)
{
    QString txt;
    int x;
    txt+=Dungeon[GetInfo(MS,MR,"Maps",3)[0].toInt()];
    x=GetInfo(MS,MR,"Maps",2)[0].toInt();
    if(x==0 || x==8 || x==9 || x==10 || x==13)
        txt+=" d'";
    else
        txt+=" de ";
    txt += Material[x];
    x = GetInfo(MS,MR,"Maps",4)[0].toInt();
    switch (x)
    {
        case 6:
        case 7:
        case 14:{txt += " du "; break;}
        case 2:
        case 3:
        case 5:
        case 9:
        case 12:
        case 13:{txt += " de l'"; break;}
        default:{txt += " de la "; break;}
    }
    txt += Difficulty[x];
    txt += " Niv. "+GetInfo(MS,MR,"Maps",5)[0];
    return txt;
}

void MainWindow::MapSelected(QListWidgetItem* item)
{
    QPushButton* pb = OpenUi->findChild<QPushButton*>("Ok");
    pb->setEnabled(true);

    MapSel = item;
}

void MainWindow::Validate()
{
    Validate(MapSel);
}

void MainWindow::Validate(QListWidgetItem* item)
{
    int a = item->listWidget()->row(item);
    //qDebug() << item->text() << a;

    QList<QStringList> List = GetTable("SavedMaps");
    MapSeed=List[0][a].toInt();
    MapRankValue=List[1][a].toInt();
    Loc=List[2][a].toInt(nullptr,16);
    MapRank=BaseRankTable[MapRankValue];

    OpenUi->close();

    CalculateDetail(true);
    CreateMap();
}

void MainWindow::CancelOpen()
{
    OpenUi->close();
}

void MainWindow::SaveMap()
{
    QList<QStringList> List = GetTable("SavedMaps");
    bool ok = false;
    if (!List.isEmpty())
    {
        for (int i=0; i<List[0].size(); i++)
        {
            if (List[0][i].toInt()==MapSeed && List[1][i].toInt()==MapRankValue)
                ok = true;
        }
        if(!ok)
            AddMap(MapSeed,MapRankValue,Loc);
    }
    else
    {
        AddMap(MapSeed,MapRankValue,Loc);
    }

    QAction* act = this->findChild<QAction*>("Save");
    act->setEnabled(false);
}

//###############################################################################################################################
//###############################                       Function Draw Maps                        ###############################
//###############################################################################################################################

void MainWindow::CreateMap()
{
    QAction* act = this->findChild<QAction*>("Monster");
    act->setEnabled(true);
    QAction* act2 = this->findChild<QAction*>("InfoMap");
    act2->setEnabled(true);

    qDeleteAll(this->findChildren<QPushButton*>());
    qDeleteAll(this->findChildren<QLabel*>());
    qDeleteAll(this->findChildren<QTableWidget*>());

    ByteFloor.resize(FloorCount());
    for(int floor = 0; floor < FloorCount(); floor++)
    {
        int floorHeight = GetFloorHeight(floor);
        int floorWidth = GetFloorWidth(floor);
        ByteFloor[floor].resize(floorHeight);
        int num = 792;
        for (int i = 0; i < floorHeight; i++)
        {
            ByteFloor[floor][i].resize(floorWidth);
            for (int j = 0; j < floorWidth; j++)
            {
                ByteFloor[floor][i][j] = GetBlockType(_dungeonInfo[floor][num+j],i,j,floor);
            }
            num += 16;
        }
    }
    QLabel* lbl = new QLabel(this);
    lbl->setObjectName("Im");

    QTableWidget* tb = new QTableWidget(this);
    tb->setObjectName("Ch");
    tb->setGeometry(695,30,505,650);
    for (int i=0; i<3; i++)
    {
        tb->insertColumn(i);
        tb->setColumnWidth(i,145);
    }
    for (int j=7; j<=3600; j++)
    {
        tb->insertRow(j-7);
        tb->setRowHeight(j-7,22);
    }
    tb->setFont(QFont ("Segoe UI", 8));
    Map();
}

uchar MainWindow::GetBlockType(uchar blockCode, int x, int y, int level)
{
    switch (blockCode)
    {
    case 1:
    case 3:
        return 0;
    case 0:
    case 2:
    case 8:
        return 3;
    case 4:
        return (uchar)(IsUpStep(level, y, x) ? 2 : 3);
    case 5:
        return 1;
    case 6:
        return (uchar)(4 + GetTreasureBoxIndex(level, y, x));
    default:
        return NULL;//throw new ArgumentOutOfRangeException("blockCode");
    }
}

void MainWindow::Map()
{
    Floor.resize(FloorCount());
    for (int i=0; i< FloorCount(); i++)
    {
        QImage* Img = new QImage((GetFloorWidth(i)-2)*48,(GetFloorHeight(i)-2)*48,QImage::Format_RGB32);
        for (int x=1; x<=GetFloorWidth(i)-2; x++)
        {
            for (int y=1; y<=GetFloorHeight(i)-2; y++)
            {
                auto tile = new TileMap(ByteFloor[i],y,x);
                QPainter p(Img);
                p.drawImage ((x-1)*48,(y-1)*48,PaintImage(tile));
            }
        }
        Floor[i]=Img->copy();
        delete Img;
    }

    FloorLevel();
}

QImage MainWindow::PaintImage(TileMap* tile)
{
    QImage* Img = new QImage(48,48,QImage::Format_RGB32);

    QPainter p(Img);
    p.drawImage (0,0,PaintTile(tile->TL));
    p.drawImage (16,0,PaintTile(tile->T));
    p.drawImage (32,0,PaintTile(tile->TR));
    p.drawImage (0,16,PaintTile(tile->L));
    p.drawImage (16,16,PaintTile(tile->M));
    p.drawImage (32,16,PaintTile(tile->R));
    p.drawImage (0,32,PaintTile(tile->BL));
    p.drawImage (16,32,PaintTile(tile->B));
    p.drawImage (32,32,PaintTile(tile->BR));

    return Img->copy();
}

QImage MainWindow::PaintTile(int offset)
{
    QColor RGB;
    QImage* img = new QImage(16,16,QImage::Format_RGB32);
    for (int x=0; x<16; x++)
    {
        for (int y=0; y<16; y++)
        {
            RGB.setRed((uchar)Color[((x*16)+y)*4+offset*1024+2]);
            RGB.setGreen((uchar)Color[((x*16)+y)*4+offset*1024+1]);
            RGB.setBlue((uchar)Color[((x*16)+y)*4+offset*1024]);
            img->setPixelColor(y,x,RGB);
        }
    }
    return img->copy();
}

void MainWindow::FloorLevel()
{
    QSignalMapper* signalMapper = new QSignalMapper(this);
    connect(signalMapper, SIGNAL(mappedInt(int)),this, SLOT(Map(int)));
    for (int i=0; i<FloorCount(); i++)
    {
        QPushButton* pb = new QPushButton(this);
        signalMapper->setMapping(pb, i);
        pb->setFlat(true);
        pb->setText(QString::number(i+1));
        connect(pb,SIGNAL(clicked()), signalMapper, SLOT (map()));
        pb->setGeometry(0,22+i*20,20,20);
        pb->show();
    }
    Map(0);
}

void MainWindow::Map(int Level)
{
    QLabel* lbl = this->findChild<QLabel*>("Im");
    lbl->setGeometry(20,22,Floor[Level].width(),Floor[Level].height());
    lbl->setPixmap(QPixmap::fromImage(Floor[Level]));
    lbl->show();
    Chest(Level);
}

void MainWindow::Chest(int Level)
{
    QTableWidget* tb = this->findChild<QTableWidget*>("Ch");
    tb->clear();
    if(Level>1)
    {
        for (int i=0; i<3; i++)
        {
            if (i<GetTreasureBoxCountPerFloor(Level))
            {
                tb->setHorizontalHeaderItem(i,new QTableWidgetItem(QIcon(QPixmap::fromImage(PaintTile(4+i))),"Rank "+QString::number(GetTreasureBoxRankPerFloor(Level,i))));
                tb->showColumn(i);
            }
            else
                tb->hideColumn(i);
        }

        QStringList txt;

        for (int j=7; j<=3600; j++)
        {
            for (int i=0; i<GetTreasureBoxCountPerFloor(Level); i++)
            {
                tb->setItem(j-7,i,new QTableWidgetItem(GetTreasureBoxItem(Level,i,j)));
            }
            int s = j%60;
            QString row = QString::number(floor(j/60));
            if (s<10)
                row += ":0"+QString::number(s);
            else
                row += ":"+QString::number(s);
            txt.append(row);
        }
        tb->setVerticalHeaderLabels(txt);

        tb->show();
    }
    else
        tb->hide();
}

//###############################################################################################################################
//###############################                    Function Access DataBase                     ###############################
//###############################################################################################################################

QList<QStringList> MainWindow::GetTable(QString Table)
{
    QList<QStringList> List;
    List.append(GetTable(Table,0));
    List.append(GetTable(Table,1));
    List.append(GetTable(Table,2));
    return List;
}

QStringList MainWindow::GetTable(QString Table, int Pos)
{
    QSqlQuery query;
    query.prepare("SELECT * FROM "+Table);
    if(!query.exec())
    {
        qWarning() << "Erreur : " << query.lastError().text();
    }
    //Value : 0 = Id, 1 = Txt, 2 = TxtFr
    QStringList List;
    while (query.next())
    {
        List += query.value(Pos).toString();
    }
    return List;
}

QStringList MainWindow::GetMaps(int Item,int MaterialId, int DungeonId, int DifficultyId, int Level)
{
    QSqlQuery query;
    QString cmd = "SELECT Maps.MapSeed, Maps.MapRank, Locations.Location FROM Maps INNER JOIN Locations ON Maps.MapSeed = Locations.MapSeed AND Maps.MapRank = Locations.MapRank WHERE Maps.MaterialTypeId = "+QString::number(MaterialId)+" AND Maps.DungeonTypeId = "+QString::number(DungeonId)+" AND Maps.DifficultyTypeId = "+QString::number(DifficultyId)+" AND Maps.Level = "+QString::number(Level);
    query.prepare(cmd);
    if(!query.exec())
    {
        qWarning() << "Erreur : " << query.lastError().text();
    }
    QStringList List;
    List.clear();
    while (query.next())
    {
        List += query.value(Item).toString();
    }
    return List;
}

QStringList MainWindow::GetMaps(uint Seed, int Rank)
{
    return GetInfo(Seed,Rank,"Locations",2);
}

QStringList MainWindow::GetInfo(uint Seed, int Rank, QString Table, int item)
{

    QSqlQuery query;
    QString cmd = "SELECT * FROM "+Table+" WHERE MapSeed = "+QString::number(Seed)+" AND MapRank = "+QString::number(Rank);
    query.prepare(cmd);
    if(!query.exec())
    {
        qWarning() << "Erreur : " << query.lastError().text();
        //qDebug () << query.lastQuery();
    }
    QStringList List;
    List.clear();
    while (query.next())
    {
        List += query.value(item).toString();
    }
    return List;
}

void MainWindow::AddMap(uint Seed, int Rank, uchar Loc)
{
    QSqlQuery query;
    QString cmd = "INSERT INTO SavedMaps VALUES ("+QString::number(Seed)+", "+QString::number(Rank)+", \""+QString::number(Loc,16)+"\")";
    query.prepare(cmd);
    if(!query.exec())
    {
        qWarning() << "Erreur : " << query.lastError().text();
        //qDebug () << query.lastQuery();
    }
}

//###############################################################################################################################
//###############################                     Function Map Selector                      ################################
//###############################################################################################################################

void MainWindow::MapSelect()
{
    MapsUi = new QWidget(this,Qt::Popup|Qt::Dialog);
    MapsUi->setWindowModality(Qt::ApplicationModal);
    MapsUi->setFixedSize(QSize(670,400));
    MapsUi->move(100,100);
    MapsUi->setWindowTitle("Selecteur de carte");
    MapsUi->show();

    for (int i=0; i<3; i++)
    {
        QComboBox* cb = new QComboBox(MapsUi);
        cb->setGeometry(20+i*140,20,130,22);
        cb->setMaxVisibleItems(25);
        if(i==0)
            cb->addItems(Dungeon);
        else if(i==1)
            cb->addItems(Material);
        else
            cb->addItems(Difficulty);
        cb->setObjectName("Id"+QString::number(i));
        cb->show();
    }

    QSpinBox* sb = new QSpinBox(MapsUi);
    sb->setGeometry(440,20,60,22);
    sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sb->setRange(1,99);
    sb->setObjectName("Lvl");
    sb->setAlignment(Qt::AlignLeft);
    sb->show();

    QScrollArea* sa = new QScrollArea(MapsUi);
    sa->setGeometry(20,60,630,280);
    sa->setObjectName("SA");
    sa->show();

    //Search
    QPushButton* pb1 = new QPushButton(MapsUi);
    pb1->setGeometry(510,20,75,22);
    pb1->setText("Chercher");
    connect(pb1, SIGNAL(clicked()), this, SLOT(Search()));
    pb1->show();

    //Ok
    QPushButton* pb2 = new QPushButton(MapsUi);
    pb2->setGeometry(500,360,75,22);
    pb2->setEnabled(false);
    pb2->setText("Ok");
    pb2->setObjectName("Ok");
    connect(pb2, SIGNAL(clicked()), this, SLOT(OK()));
    pb2->show();

    //Cancel
    QPushButton* pb3 = new QPushButton(MapsUi);
    pb3->setGeometry(580,360,75,22);
    pb3->setText("Annuler");
    connect(pb3, SIGNAL(clicked()), this, SLOT(Cancel()));
    pb3->show();
}

void MainWindow::Search()
{
    // Dungeon = Id0, Material = Id1, Difficulty = Id2 et Level = Lvl

    QList<QComboBox*> childcb;
    for (int i=0; i<3; i++)
    {
        childcb.append(MapsUi->findChild<QComboBox*>("Id"+QString::number(i)));
    }

    QSpinBox* sb = MapsUi->findChild<QSpinBox*>("Lvl");

    int DungeonId=childcb[0]->currentIndex();
    int MaterialId=childcb[1]->currentIndex();
    int DifficultyId=childcb[2]->currentIndex();
    int Level=sb->value();

    Seed = GetMaps(0, MaterialId, DungeonId, DifficultyId, Level);
    Rank = GetMaps(1, MaterialId, DungeonId, DifficultyId, Level);
    Location = GetMaps(2, MaterialId, DungeonId, DifficultyId, Level);

    QPushButton* pbok = MapsUi->findChild<QPushButton*>("Ok");
    pbok->setEnabled(false);

    Maps(Location);
}

void MainWindow::Maps(QStringList Location)
{
    QWidget *Map = new QWidget;
    QGridLayout *g = new QGridLayout(Map);
    QSignalMapper* signalMapper = new QSignalMapper(this);
    connect(signalMapper, SIGNAL(mappedInt(int)),this, SLOT(Selected(int)));
    for(int i=0; i<Location.size(); i++){
        QString path=":/Image/"+QString::number(Location[i].toInt(),16).toUpper()+".gif";
        QPixmap px(path);
        px = px.scaled(240,180);
        QPushButton* pb = new QPushButton(MapsUi);
        signalMapper->setMapping(pb, i);
        pb->setIconSize(QSize(240,180));
        pb->setIcon(px);
        pb->setFixedSize(QSize(240,180));
        pb->setFlat(true);
        pb->setObjectName("Map"+QString::number(i));
        connect(pb,SIGNAL(clicked()), signalMapper, SLOT (map()));
        g->addWidget(pb,i/2,i%2,Qt::AlignCenter);
    }

    QScrollArea* SA = MapsUi->findChild<QScrollArea*>("SA");
    SA->setWidget(Map);
}

void MainWindow::OK()
{
    MapSeed = Seed[pos].toInt();
    MapRankValue = Rank[pos].toInt();
    MapRank = BaseRankTable[Rank[pos].toInt()];
    Loc=Location[pos].toInt();
    MapsUi->close();

    QAction* act = this->findChild<QAction*>("Save");
    act->setEnabled(true);

    CalculateDetail(true);
    CreateMap();
}

void MainWindow::Selected(int position)
{
    QPushButton* child;
    if(!Name.isEmpty())
    {
        child = MapsUi->findChild<QPushButton*>(Name);
        child->setIcon(Image);
    }

    pos=position;
    Name="Map"+QString::number(position);
    child = MapsUi->findChild<QPushButton*>(Name);
    Image = child->icon().pixmap(240,180).copy();
    QImage image = Image.toImage();

    QPainter* qPainter = new QPainter(&image);// all qPainter to draw rectangle on original image according to point get from Filter image
    qPainter->begin(MapsUi);
    qPainter->setPen(QPen(Qt::red,4,Qt::SolidLine));
    qPainter->drawRect(2,2,image.width()-4,image.height()-4);
    qPainter->end();

    child->setIcon(QPixmap::fromImage(image));

    QPushButton* pbok = MapsUi->findChild<QPushButton*>("Ok");
    pbok->setEnabled(true);
}

void MainWindow::Cancel()
{
    MapsUi->close();
}

//###############################################################################################################################
//###############################                   Function Calculate Dungeon                   ################################
//###############################################################################################################################

void MainWindow::TreasureMapDetailData()
{
    for (int i = 0; i < 16; i++)
    {
        _treasureBoxInfoList[i].clear();
    }
    if (!_candidateRank.isEmpty())
    {
        return;
    }
    for (int j = 1; j <= 99; j++)
    {
        for (int k = 0; k <= 50; k += 5)
        {
            for (int l = 1; l <= 99; l++)
            {
                uchar item = (uchar)(j + k + l);
                if (!_candidateRank.contains(item))
                {
                    _candidateRank.append(item);
                }
            }
        }
    }
}

void MainWindow::CreateReverseSeedTable()
{
    for (uint num = 0u; num < 65536; num++)
    {
        uint num2 = num * 1103515245 + 12345;
        num2 = ((num2 * 1103515245 + 12345) >> 16) & 0x7FFFu;
        if(_reverseSeedTable.contains((ushort)num2))
        {
            _reverseSeedTable[(ushort)num2].append((ushort)num);
            continue;
        }
        QList<ushort> list;
        list.append((ushort)num);
        _reverseSeedTable.insert((ushort)num2, list);
    }
}

uint MainWindow::GenerateRandom()
{
    _seed *= 1103515245u;
    _seed += 12345u;
    return (_seed >> 16) & 0x7FFFu;
}

uint MainWindow::GenerateRandom(uint value)
{
    uint num = GenerateRandom();
    return num % value;
}

uint MainWindow::GenerateRandom(uint value1, uint value2)
{
    if (value1 == value2)
    {
        return value1;
    }
    uint num = GenerateRandom();
    int num2 = (int)(value2 - value1 + 1);
    if (num2 == 0)
    {
        return value1;
    }
    return (uint)(value1 + (long)num % (long)num2);
}

void MainWindow::CalculateDetail()
{
    CalculateDetail(false, false);
}

void MainWindow::CalculateDetail(bool floorDetail)
{
    CalculateDetail(floorDetail, false);
}

void MainWindow::CalculateDetail(bool floorDetail, bool createSearchData)
{
    _validSeed = false;
    _validPlaceList.clear();
    _lowRankValidPlaceList.clear();
    _middleRankValidPlaceList.clear();
    _highRankValidPlaceList.clear();
    _validRankList.clear();
    for (int i = 0; i < 16; i++)
    {
        _treasureBoxInfoList[i].clear();
    }
    memset(_details, 0, 20);
    memset(_details2, 0, 20);
    /*Array.Clear(_details, 0, 20);
    Array.Clear(_details2, 0, 20);*/
    if (MapRank < 2 || MapRank > 248)
    {
        return;
    }
    _seed = MapSeed;
    for (int j = 0; j < 12; j++)
    {
        GenerateRandom(100u);
    }
    _details[3] = (uchar)Seek1(TableA, 5);
    _details[1] = (uchar)Seek2(TableB, MapRank, 9);
    _details[2] = (uchar)Seek2(TableC, MapRank, 8);
    _details[0] = (uchar)Seek4(TableD, TableE, 9);
    for (int k = 0; k < 12; k++)
    {
        _details[k + 1 + 7] = (uchar)Seek3(TableF[k * 4 + 1], TableF[k * 4 + 2]);
    }
    _details[5] = (uchar)Seek2(TableH, _details[2], 5);
    _details[6] = (uchar)Seek2(TableI, _details[0], 4);
    _details[7] = (uchar)Seek2(TableG, _details[1], 8);
    int num = (_details[0] + _details[1] + _details[2] - 4) * 3;
    num += (int)(GenerateRandom(11u) - 5);
    if (num < 1)
    {
        num = 1;
    }
    if (num > 99)
    {
        num = 99;
    }
    _details[4] = (uchar)num;
    _name3Index = TreasureMapName3_IndexTable[(_details[7] - 1) * 5 + _details[3] - 1];
    if (!createSearchData)
    {
        QList<ushort> reverseSeedTable = GetReverseSeedTable(MapSeed);
        if (!reverseSeedTable.empty())
        {
            _validSeed = true;
            for(ushort item : reverseSeedTable)
            {
                ushort num2 = (ushort)(_seed = item);
                uint num3 = GenerateRandom();

                for(ushort item2 : _candidateRank)
                {
                    int num4 = (int)(item2 + (long)num3 % (long)((int)item2 / 10 * 2 + 1) - (int)item2 / 10);
                    if (num4 > 248)
                    {
                        num4 = 248;
                    }
                    if (!_validRankList.contains((uchar)num4))
                    {
                        _validRankList.append((uchar)num4);
                    }
                }
                num3 = GenerateRandom();
                uint num5 = ((MapRank <= 50) ? (num3 % 47u + 1) : ((MapRank > 80) ? (num3 % 150u + 1) : (num3 % 131u + 1)));
                if (!_validPlaceList.contains((uchar)num5))
                {
                    _validPlaceList.append((uchar)num5);
                }
                num5 = num3 % 47u + 1;
                if (!_lowRankValidPlaceList.contains((uchar)num5))
                {
                    _lowRankValidPlaceList.append((uchar)num5);
                }
                num5 = num3 % 131u + 1;
                if (!_middleRankValidPlaceList.contains((uchar)num5))
                {
                    _middleRankValidPlaceList.append((uchar)num5);
                }
                num5 = num3 % 150u + 1;
                if (!_highRankValidPlaceList.contains((uchar)num5))
                {
                    _highRankValidPlaceList.append((uchar)num5);
                }

            }

            if (MapRank == 2 && MapSeed == 50)
            {
                _validPlaceList.append(5);
                _lowRankValidPlaceList.append(5);
            }
        }
        else
        {
            _validSeed = false;
            _validPlaceList.clear();
            _validRankList.clear();
        }
    }
    if (floorDetail)
    {
        for (int l = 1; l < _details[1] + 1; l++)
        {
            int num6 = ((l > 4) ? ((l > 8) ? ((l > 12) ? 16 : ((MapSeed + l) % 3 + 14)) : ((MapSeed + l) % 4 + 12)) : ((MapSeed + l) % 5 + 10));
            _dungeonInfo[l - 1][2] = (uchar)num6;
            _dungeonInfo[l - 1][3] = (uchar)num6;
        }
        CreateDungeonDetail(createSearchData);
    }
}

uint MainWindow::Seek1(uchar* table, int tableSize)
{
    uint num = GenerateRandom(100u);
    uint num2 = 0u;
    for (int i = 0; i < tableSize; i++)
    {
        num2 += table[i * 4 + 1];
        if (num < num2)
        {
            return table[i * 4];
        }
    }
    return 0u;
}

uint MainWindow::Seek2(uchar* table, uchar value, int tableSize)
{
    for (int i = 0; i < tableSize; i++)
    {
        if (value >= table[i * 4] && value <= table[i * 4 + 1])
        {
            return Seek3(table[i * 4 + 2], table[i * 4 + 3]);
        }
    }
    return 0u;
}

uint MainWindow::Seek3(uchar val1, uchar val2)
{
    uint num = GenerateRandom();
    int num2 = val2 - val1 + 1;
    if (num2 == 0)
    {
        return val1;
    }
    return (uint)(val1 + (long)num % (long)num2);
}

uint MainWindow::Seek4(uchar* table1, uchar* table2, int roopCount)
{
    for (int i = 0; i < roopCount; i++)
    {
        if (MapRank < table1[i * 4] || MapRank > table1[i * 4 + 1])
        {
            continue;
        }
        int num = 0;
        for (int j = table1[i * 4 + 2]; j <= table1[i * 4 + 3]; j++)
        {
            num += table2[(j - 1) * 2 + 1];
        }
        int num2 = (int)((long)GenerateRandom() % (long)num);
        num = 0;
        for (int k = table1[i * 4 + 2]; k <= table1[i * 4 + 3]; k++)
        {
            num += table2[(k - 1) * 2 + 1];
            if (num2 < num)
            {
                return (uint)k;
            }
        }
        break;
    }
    return 0u;
}

QList<ushort> MainWindow::GetReverseSeedTable(ushort seed)
{
    if (_reverseSeedTable.contains(seed))
    {
        return _reverseSeedTable[seed];
    }
    QList<ushort> l;
    l.clear();
    return l;
}

void MainWindow::CreateDungeonDetail(bool createSearchData)
{
    int num = 0;
    for (int i = 1; i < _details[1] + 1; i++)
    {
        int num2 = i - 1;
        _dungeonInfo[num2][0] = (uchar)i;
        _dungeonInfo[num2][8] = 0;
        _seed = (uint)(MapSeed + i);
        if (createSearchData && i <= 2)
        {
            continue;
        }
        routine1(num2, 792u, 1u, 256u);
        _dungeonInfo[num2][21] = 1;
        _dungeonInfo[num2][22] = 0;
        _dungeonInfo[num2][23] = 0;
        _dungeonInfo[num2][1] = 0;
        SetValue(num2, 24u, 1, 1, (uchar)(_dungeonInfo[num2][2] - 2), (uchar)(_dungeonInfo[num2][3] - 2));
        _dungeonInfo[num2][28] = 0;
        _dungeonInfo[num2][29] = 0;
        routineF(num2, 24u);
        for (int j = 0; j < _dungeonInfo[num2][21]; j++)
        {
            if (routineC(num2, (uint)(j * 12 + 24), (uint)(_dungeonInfo[num2][23] * 20 + 472)))
            {
                    _dungeonInfo[num2][23]++;
            }
        }
        for (int k = 0; k < _dungeonInfo[num2][21]; k++)
        {
            GenerateFloorMap(num2, (uint)(k * 12 + 24));
        }
        for (int l = 0; l < _dungeonInfo[num2][22]; l++)
        {
            routineG(num2, (uint)((l << 4) + 216));
        }
        for (int m = 0; m < _dungeonInfo[num2][2]; m++)
        {
            _dungeonInfo[num2][m + 792] = 1;
            _dungeonInfo[num2][((_dungeonInfo[num2][3] - 1) << 4) + m + 792] = 1;
        }
        for (int n = 0; n < _dungeonInfo[num2][3]; n++)
        {
            _dungeonInfo[num2][(n << 4) + 792] = 1;
            _dungeonInfo[num2][(n << 4) + 792 + _dungeonInfo[num2][2] - 1] = 1;
        }
        routineJ(num2);
        if (_dungeonInfo[num2][0] <= 2)
        {
            _dungeonInfo[num2][8] = 0;
        }
        else
        {
            routineK(num2);
        }
        if (createSearchData)
        {
            continue;
        }
        _dungeonInfo[num2][1306] = 0;
        _dungeonInfo[num2][1307] = 0;
        for (int num3 = 1; num3 < 15; num3++)
        {
        for (int num4 = 1; num4 < 15; num4++)
            {
                int num5 = _dungeonInfo[num2][num4 + (num3 << 4) + 792];
                if (num5 != 1 && num5 != 3)
                {
                    _dungeonInfo[num2][1306]++;
                    num = _dungeonInfo[num2][ num4 + ((num3 - 1) << 4) + 792];
                    if (num != 1 && num != 3)
                    {
                        _dungeonInfo[num2][1307]++;
                    }
                    num = _dungeonInfo[num2][(num3 << 4) + 792 + num4 - 1];
                    if (num != 1 && num != 3)
                    {
                        _dungeonInfo[num2][1307]++;
                    }
                }
            }
        }
        _dungeonInfo[num2][1304] = (uchar)(_details[2] + (i - 1) / 4);
        int num6 = (TableJ[_details[3] - 1] * 12 + _dungeonInfo[num2][1304] - 1) * 18;
        _dungeonInfo[num2][1305] = TableK[num6];
        _dungeonInfo[num2][1312] = 0;
        uchar* ptr = &_dungeonInfo[num2][0];
        *(int*)(ptr + 1308) = (ptr[1306] << 4) + 4896;
        int num7 = 4128 - ((ptr[1306] << 4) + ptr[1307] * 8);
        if (num7 < 0)
        {
            *(int*)(ptr + 1308) += (ptr[1307] + (num7 + 7) / 8 - 1) * 8;
        }
        else
        {
            *(int*)(ptr + 1308) += ptr[1307] * 8;
        }
        *(int*)(ptr + 1308) += 4;
        *(int*)(ptr + 1308) += ptr[1307] * 8;
        *(int*)(ptr + 1308) += ptr[1305] * 20;
        for (int num8 = 0; num8 < ptr[1305]; num8++)
        {
            uchar* ptr2 = &TableK[0];
            int num9 = *(ushort*)(ptr2 + (num6 + num8 * 2 + 2));
            uchar* ptr3 = &TableM[num9 * 2];
            *(int*)(ptr + 1308) += TableL[*(ushort*)ptr3] + 8;
            num = 11216;
            num = (((uint)num >= (uint)(*(int*)(ptr + 1308))) ? ((int)(num - (uint)(*(int*)(ptr + 1308))) / 20) : 0);
            ptr[1313] = 0;
            int num10 = 0;
            for (int num11 = 0; num11 < ptr[1305]; num11++)
            {
                try
                {
                    uchar* ptr4 = &TableK[0];
                    int num12 = *(ushort*)(ptr4 + (num6 + num11 * 2 + 2));
                    if (num12 < 38 || num12 > 40)
                    {
                        if (num11 >= num)
                        {
                            num10++;
                            continue;
                        }
                        *(ushort*)(ptr + (ptr[1313] * 2 + 1314)) = (ushort)num12;
                        uchar* num13 = ptr + 1313;
                        *num13 = (uchar)(*num13 + 1);
                    }
                }
                catch (std::out_of_range) {}
            }
            if (ptr[1313] == 0)
            {
                ptr[1312] = (uchar)(ptr[1312] | 1u);
            }
            else if (ptr[1313] == 1)
            {
                ptr[1312] = (uchar)(ptr[1312] | 2u);
            }
            else if (num10 > 0)
            {
                ptr[1312] = (uchar)(ptr[1312] | 4u);
            }
            if (ptr[1313] == 1)
            {
                _details2[14] = ptr[1314];
                _details2[15] = ptr[1315];
            }
            else
            {
                _details2[12] = (uchar)(_details2[12] | ptr[1312]);
            }
        }
    }
    for (int num14 = 2; num14 < _details[1]; num14++)
    {
        _seed = (uint)(MapSeed + num14 + 1);
        for (int num15 = 0; num15 < _dungeonInfo[num14][8] << 1; num15++)
        {
            GenerateRandom();
        }
        num = _details[2] + num14 / 4;
        for (int num16 = 0; num16 < _dungeonInfo[num14][8]; num16++)
        {
            _dungeonInfo[num14][num16 + 9] = (uchar)GetItemRank(TableN[(num - 1) * 4 + 1], TableN[(num - 1) * 4 + 2]);
            _details2[_dungeonInfo[num14][num16 + 9] - 1]++;
        }
        if (_dungeonInfo[num14][8] <= 0)
        {
            continue;
        }
        for (int num17 = 0; num17 < _dungeonInfo[num14][8]; num17++)
        {
            //TreasureBoxInfo treasureBoxInfo = new TreasureBoxInfo(num17, _dungeonInfo[num14][9 + num17], _dungeonInfo[num14][num17 * 2 + 13], _dungeonInfo[num14][num17 * 2 + 14]);
            TreasureBoxInfo treasureBoxInfo = {num17, _dungeonInfo[num14][9 + num17], _dungeonInfo[num14][num17 * 2 + 13], _dungeonInfo[num14][num17 * 2 + 14]};
            int num18;
            for (num18 = 0; num18 < _treasureBoxInfoList[num14].count() && treasureBoxInfo.rank <= _treasureBoxInfoList[num14][num18].rank; num18++)
            {
            }
            _treasureBoxInfoList[num14].insert(num18, treasureBoxInfo);
        }
    }
    //qDebug() << "Fin";
}

void MainWindow::SetValue(int floor, uint address, uchar value1, uchar value2, uchar value3, uchar value4)
{
    _dungeonInfo[floor][address] = value1;
    _dungeonInfo[floor][address + 1] = value2;
    _dungeonInfo[floor][address + 2] = value3;
    _dungeonInfo[floor][address + 3] = value4;
}

void MainWindow::routine1(int floor, uint address, uint value1, uint value2)
{
    if (value2 == 0)
    {
        return;
    }
    uint num = value1;
    if (value2 >= 4)
    {
        num = (value1 << 8) + value1;
        uint num2 = num;
        num <<= 16;
        num += num2;
        for (num2 = value2 >> 2; num2 != 0; num2--)
        {
            uchar* ptr = &_dungeonInfo[floor][address];
            *(uint*)ptr = num;
            address += 4;
        }
    }
    if (value2 < 4 || (value2 & 3u) != 0)
    {
        for (value2 &= 3u; value2 != 0; value2--)
        {
            _dungeonInfo[floor][address] = (uchar)num;
            address++;
        }
    }
}

bool MainWindow::routineA(int floor, uint address, uint value1, uint value2)
{
    int num = _dungeonInfo[floor][address + 3] + 1 - _dungeonInfo[floor][address + 1];
    if (num < 7)
    {
        return false;
    }
    if (_dungeonInfo[floor][address + 4] != 0)
    {
        return false;
    }
    num = (int)GenerateRandom(0u, (uint)(num - 7));
    int num2 = _dungeonInfo[floor][address + 1] + num + 3;
    for (int i = _dungeonInfo[floor][address]; i < _dungeonInfo[floor][address + 2] + 1; i++)
    {
        _dungeonInfo[floor][(num2 << 4) + i + 792] = 3;
    }
    SetValue(floor, value2, _dungeonInfo[floor][address], (uchar)num2, _dungeonInfo[floor][address + 2], (uchar)num2);
    SetValue(floor, value1, _dungeonInfo[floor][address], (uchar)(num2 + 1), _dungeonInfo[floor][address + 2], _dungeonInfo[floor][address + 3]);
    _dungeonInfo[floor][value1 + 4] = 0;
    _dungeonInfo[floor][value1 + 5] = 0;
    _dungeonInfo[floor][address + 3] = (uchar)(num2 - 1);
    _dungeonInfo[floor][address + 4] = 1;
    uchar* ptr = &_dungeonInfo[floor][value2];
    *(uint*)(ptr + 4) = address;
    *(uint*)(ptr + 8) = value1;
    _dungeonInfo[floor][value2 + 12] = 1;
    return true;
}

void MainWindow::routineB(int floor, uint address)
{
    int num = _dungeonInfo[floor][21];
    _dungeonInfo[floor][21]++;
    _dungeonInfo[floor][22]++;
    if ((GenerateRandom() & (true ? 1u : 0u)) != 0)
    {
        routineF(floor, address);
        routineF(floor, (uint)(num * 12 + 24));
    }
    else
    {
        routineF(floor, (uint)(num * 12 + 24));
        routineF(floor, address);
    }
}

bool MainWindow::routineC(int floor, uint address1, uint address2)
{
    int num = _dungeonInfo[floor][address1 + 2] + 1 - _dungeonInfo[floor][address1];
    if (num < 3)
    {
        return false;
    }
    num = _dungeonInfo[floor][address1 + 3] + 1 - _dungeonInfo[floor][address1 + 1];
    if (num < 3)
    {
        return false;
    }
    int num2 = _dungeonInfo[floor][address1];
    int num3 = _dungeonInfo[floor][address1 + 1];
    int num4 = _dungeonInfo[floor][address1 + 2];
    int num5 = _dungeonInfo[floor][address1 + 3];
    int num6;
    int num7;
    if (GenerateRandom(0u, 1u) != 0)
    {
        num6 = (int)GenerateRandom((uint)num2, (uint)(num2 + (num4 - num2 + 1) / 3));
        num7 = (int)GenerateRandom((uint)num3, (uint)(num3 + (num5 - num3 + 1) / 3));
    }
    else
    {
        num6 = (int)GenerateRandom((uint)(num2 + 1), (uint)(num2 + (num4 - num2 + 1) / 3));
        num7 = (int)GenerateRandom((uint)(num3 + 1), (uint)(num3 + (num5 - num3 + 1) / 3));
    }
    int num8;
    int num9;
    if (GenerateRandom(0u, 1u) != 0)
    {
        num8 = (int)GenerateRandom((uint)(num2 + (num4 - num2 + 1) / 3 * 2), (uint)num4);
        num9 = (int)GenerateRandom((uint)(num3 + (num5 - num3 + 1) / 3 * 2), (uint)num5);
    }
    else
    {
        num8 = (int)GenerateRandom((uint)(num2 + (num4 - num2 + 1) / 3 * 2), (uint)(num4 - 1));
        num9 = (int)GenerateRandom((uint)(num3 + (num5 - num3 + 1) / 3 * 2), (uint)(num5 - 1));
    }
    SetValue(floor, address2, (uchar)num6, (uchar)num7, (uchar)num8, (uchar)num9);
    for (int i = 4; i < 20; i++)
    {
        _dungeonInfo[floor][address2 + i] = 0;
    }
    uchar* ptr = &_dungeonInfo[floor][address1 + 8];
    *(uint*)ptr = address2;
    for (int j = num7; j <= num9; j++)
    {
        for (int k = num6; k <= num8; k++)
        {
            _dungeonInfo[floor][k + (j << 4) + 792] = 0;
        }
    }
    routineD(floor, address2);
    return true;
}

bool MainWindow::routineD(int floor, uint address)
{
    int num = _dungeonInfo[floor][address];
    if (num == 0)
    {
        return false;
    }
    if (_dungeonInfo[floor][address + 1] == 0)
    {
        return false;
    }
    int num2 = _dungeonInfo[floor][address + 2];
    if (num2 == 0)
    {
        return false;
    }
    if (_dungeonInfo[floor][address + 3] == 0)
    {
        return false;
    }
    if (num2 - num + 1 < 5)
    {
        _dungeonInfo[floor][address + 12] = (uchar)GenerateRandom((uint)num, (uint)num2);
        _dungeonInfo[floor][address + 13] = _dungeonInfo[floor][address + 1];
        _dungeonInfo[floor][address + 16] = (uchar)GenerateRandom(_dungeonInfo[floor][address], _dungeonInfo[floor][address + 2]);
        _dungeonInfo[floor][address + 17] = _dungeonInfo[floor][address + 3];
        int num3 = (_dungeonInfo[floor][address + 13] << 4) + 792;
        num3 += _dungeonInfo[floor][address + 12];
        _dungeonInfo[floor][num3] = 8;
        num3 = (_dungeonInfo[floor][address + 17] << 4) + 792;
        num3 += _dungeonInfo[floor][address + 16];
        _dungeonInfo[floor][num3] = 8;
    }
    else
    {
        int num4 = num + (num2 - num + 1) / 2 - 1;
        _dungeonInfo[floor][address + 12] = (uchar)GenerateRandom((uint)num, (uint)num4);
        _dungeonInfo[floor][address + 13] = _dungeonInfo[floor][address + 1];
        _dungeonInfo[floor][address + 14] = (uchar)GenerateRandom((uint)(num4 + 1), _dungeonInfo[floor][address + 2]);
        _dungeonInfo[floor][address + 15] = _dungeonInfo[floor][address + 1];
        _dungeonInfo[floor][address + 16] = (uchar)GenerateRandom(_dungeonInfo[floor][address], (uint)num4);
        _dungeonInfo[floor][address + 17] = _dungeonInfo[floor][address + 3];
        _dungeonInfo[floor][address + 18] = (uchar)GenerateRandom((uint)(num4 + 1), _dungeonInfo[floor][address + 2]);
        _dungeonInfo[floor][address + 19] = _dungeonInfo[floor][address + 3];
        int num5 = (_dungeonInfo[floor][address + 13] << 4) + 792;
        num5 += _dungeonInfo[floor][address + 12];
        _dungeonInfo[floor][num5] = 8;
        num5 = (_dungeonInfo[floor][address + 15] << 4) + 792;
        num5 += _dungeonInfo[floor][address + 14];
        _dungeonInfo[floor][num5] = 8;
        num5 = (_dungeonInfo[floor][address + 17] << 4) + 792;
        num5 += _dungeonInfo[floor][address + 16];
        _dungeonInfo[floor][num5] = 8;
        num5 = (_dungeonInfo[floor][address + 19] << 4) + 792;
        num5 += _dungeonInfo[floor][address + 18];
        _dungeonInfo[floor][num5] = 8;
    }
    if (_dungeonInfo[floor][address + 3] - _dungeonInfo[floor][address + 1] + 1 < 5)
    {
        _dungeonInfo[floor][address + 4] = _dungeonInfo[floor][address];
        _dungeonInfo[floor][address + 5] = (uchar)GenerateRandom(_dungeonInfo[floor][address + 1], _dungeonInfo[floor][address + 3]);
        _dungeonInfo[floor][address + 8] = _dungeonInfo[floor][address + 2];
        _dungeonInfo[floor][address + 9] = (uchar)GenerateRandom(_dungeonInfo[floor][address + 1], _dungeonInfo[floor][address + 3]);
        int num6 = (_dungeonInfo[floor][address + 5] << 4) + 792;
        num6 += _dungeonInfo[floor][address + 4];
        _dungeonInfo[floor][num6] = 8;
        num6 = (_dungeonInfo[floor][address + 9] << 4) + 792;
        num6 += _dungeonInfo[floor][address + 8];
        _dungeonInfo[floor][num6] = 8;
    }
    else
    {
        int num7 = _dungeonInfo[floor][address + 1];
        num7 = num7 + (_dungeonInfo[floor][address + 3] + 1 - num7) / 2 - 1;
        _dungeonInfo[floor][address + 4] = _dungeonInfo[floor][address];
        _dungeonInfo[floor][address + 5] = (uchar)GenerateRandom(_dungeonInfo[floor][address + 1], (uint)num7);
        _dungeonInfo[floor][address + 6] = _dungeonInfo[floor][address];
        _dungeonInfo[floor][address + 7] = (uchar)GenerateRandom((uint)(num7 + 1), _dungeonInfo[floor][address + 3]);
        _dungeonInfo[floor][address + 8] = _dungeonInfo[floor][address + 2];
        _dungeonInfo[floor][address + 9] = (uchar)GenerateRandom(_dungeonInfo[floor][address + 1], (uint)num7);
        _dungeonInfo[floor][address + 10] = _dungeonInfo[floor][address + 2];
        _dungeonInfo[floor][address + 11] = (uchar)GenerateRandom((uint)(num7 + 1), _dungeonInfo[floor][address + 3]);
        int num8 = (_dungeonInfo[floor][address + 5] << 4) + 792;
        num8 += _dungeonInfo[floor][address + 4];
        _dungeonInfo[floor][num8] = 8;
        num8 = (_dungeonInfo[floor][address + 7] << 4) + 792;
        num8 += _dungeonInfo[floor][address + 6];
        _dungeonInfo[floor][num8] = 8;
        num8 = (_dungeonInfo[floor][address + 9] << 4) + 792;
        num8 += _dungeonInfo[floor][address + 8];
        _dungeonInfo[floor][num8] = 8;
        num8 = (_dungeonInfo[floor][address + 11] << 4) + 792;
        num8 += _dungeonInfo[floor][address + 10];
        _dungeonInfo[floor][num8] = 8;
    }
    return true;
}

bool MainWindow::routineE(int floor, uint address, uint value1, uint value2)
{
    int num = _dungeonInfo[floor][address + 2] + 1 - _dungeonInfo[floor][address];
    if (num < 7)
    {
        return false;
    }
    if (_dungeonInfo[floor][address + 5] != 0)
    {
        return false;
    }
    num = (int)GenerateRandom(0u, (uint)(num - 7));
    int num2 = _dungeonInfo[floor][address] + num + 3;
    for (int i = _dungeonInfo[floor][address + 1]; i < _dungeonInfo[floor][address + 3] + 1; i++)
    {
        _dungeonInfo[floor][(i << 4) + num2 + 792] = 3;
    }
    SetValue(floor, value2, (uchar)num2, _dungeonInfo[floor][address + 1], (uchar)num2, _dungeonInfo[floor][address + 3]);
    SetValue(floor, value1, (uchar)(num2 + 1), _dungeonInfo[floor][address + 1], _dungeonInfo[floor][address + 2], _dungeonInfo[floor][address + 3]);
    _dungeonInfo[floor][value1 + 4] = 0;
    _dungeonInfo[floor][value1 + 5] = 0;
    _dungeonInfo[floor][address + 2] = (uchar)(num2 - 1);
    _dungeonInfo[floor][address + 5] = 1;
    uchar* ptr = &_dungeonInfo[floor][value2];
    *(uint*)(ptr + 4) = address;
    *(uint*)(ptr + 8) = value1;
    _dungeonInfo[floor][value2 + 12] = 2;
    return true;
}

void MainWindow::routineF(int floor, uint address)
{
    if (_dungeonInfo[floor][21] >= 15)
    {
        return;
    }
    if (_dungeonInfo[floor][address + 5] != 0)
    {
        if (routineA(floor, address, (uint)(_dungeonInfo[floor][21] * 12 + 24), (uint)((_dungeonInfo[floor][22] << 4) + 216)))
        {
            routineB(floor, address);
        }
    }
    else if (_dungeonInfo[floor][address + 4] != 0)
    {
        if (routineE(floor, address, (uint)(_dungeonInfo[floor][21] * 12 + 24), (uint)((_dungeonInfo[floor][22] << 4) + 216)))
        {
            routineB(floor, address);
        }
    }
    else if ((GenerateRandom() & (true ? 1u : 0u)) != 0)
    {
        if (routineE(floor, address, (uint)(_dungeonInfo[floor][21] * 12 + 24), (uint)((_dungeonInfo[floor][22] << 4) + 216)))
        {
            routineB(floor, address);
        }
    }
    else if (routineA(floor, address, (uint)(_dungeonInfo[floor][21] * 12 + 24), (uint)((_dungeonInfo[floor][22] << 4) + 216)))
    {
        routineB(floor, address);
    }
}

int MainWindow::routineG(int floor, uint address)
{
    if (_dungeonInfo[floor][address + 12] == 1)
    {
        int address2;
        int address3;
        uchar* ptr = &_dungeonInfo[floor][0];
        address2 = *(int*)(ptr + *(int*)(ptr + (int)address + 4) + 8);
        address3 = *(int*)(ptr + *(int*)(ptr + (int)address + 8) + 8);
        int value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 16u, (uint)address3, 12u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 18u, (uint)address3, 12u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 16u, (uint)address3, 14u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 18u, (uint)address3, 14u, address, (uint)value);
    }
    else if (_dungeonInfo[floor][address + 12] == 2)
    {
        int address2;
        int address3;
        uchar* ptr2 = &_dungeonInfo[floor][0];
        address2 = *(int*)(ptr2 + *(int*)(ptr2 + (int)address + 4) + 8);
        address3 = *(int*)(ptr2 + *(int*)(ptr2 + (int)address + 8) + 8);
        int value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 8u, (uint)address3, 4u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 10u, (uint)address3, 4u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 8u, (uint)address3, 6u, address, (uint)value);
        value = ((GenerateRandom(0u, 7u) == 0) ? 1 : 0);
        routineH(floor, (uint)address2, 10u, (uint)address3, 6u, address, (uint)value);
    }
    return 1;
}

bool MainWindow::routineH(int floor, uint address1, uint value1, uint address2, uint value2, uint address3, uint value3)
{
    int num = _dungeonInfo[floor][address1 + value1];
    int num2 = _dungeonInfo[floor][address1 + value1 + 1];
    int num3 = _dungeonInfo[floor][address2 + value2];
    int num4 = _dungeonInfo[floor][address2 + value2 + 1];
    if (num == 0 || num2 == 0 || num3 == 0 || num4 == 0)
    {
        return false;
    }
    if (_dungeonInfo[floor][address3 + 12] == 1)
    {
        for (int i = num2 + 1; i < _dungeonInfo[floor][address3 + 1] + 1; i++)
        {
            _dungeonInfo[floor][(i << 4) + 792 + num] = 2;
        }
        for (int num5 = num4 - 1; num5 > _dungeonInfo[floor][address3 + 1]; num5--)
        {
            _dungeonInfo[floor][(num5 << 4) + 792 + num3] = 2;
        }
        if (num < num3)
        {
            for (int j = num; j < num3 + 1; j++)
            {
                _dungeonInfo[floor][j + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
            }
        }
        else if (num > num3)
        {
            for (int k = num3; k < num + 1; k++)
            {
                _dungeonInfo[floor][k + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
            }
        }
        if (value3 == 0)
        {
            return true;
        }
        if (num < num3)
        {
            for (int l = num3 + 1; l < 16; l++)
            {
                int num6 = _dungeonInfo[floor][l + (_dungeonInfo[floor][address3 + 1] << 4) + 792];
                if (num6 != 1 && num6 != 3)
                {
                    for (int m = num3 + 1; m < l; m++)
                    {
                        _dungeonInfo[floor][m + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
                    }
                    break;
                }
            }
            for (int num7 = num - 1; num7 >= 0; num7--)
            {
                int num8 = _dungeonInfo[floor][num7 + (_dungeonInfo[floor][address3 + 1] << 4) + 792];
                if (num8 != 1 && num8 != 3)
                {
                    for (int num9 = num - 1; num9 > num7; num9--)
                    {
                        _dungeonInfo[floor][num9 + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
                    }
                    break;
                }
            }
        }
        else if (num >= num3)
        {
            for (int n = num + 1; n < 16; n++)
            {
                int num10 = _dungeonInfo[floor][n + (_dungeonInfo[floor][address3 + 1] << 4) + 792];
                if (num10 != 1 && num10 != 3)
                {
                    for (int num11 = num + 1; num11 < n; num11++)
                    {
                        _dungeonInfo[floor][num11 + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
                    }
                    break;
                }
            }
            for (int num12 = num3 - 1; num12 >= 0; num12--)
            {
                int num13 = _dungeonInfo[floor][num12 + (_dungeonInfo[floor][address3 + 1] << 4) + 792];
                if (num13 != 1 && num13 != 3)
                {
                    for (int num14 = num3 - 1; num14 > num12; num14--)
                    {
                        _dungeonInfo[floor][num14 + (_dungeonInfo[floor][address3 + 1] << 4) + 792] = 2;
                    }
                    break;
                }
            }
        }
    }
    else if (_dungeonInfo[floor][address3 + 12] == 2)
    {
        for (int num15 = num + 1; num15 < _dungeonInfo[floor][address3] + 1; num15++)
        {
            _dungeonInfo[floor][num15 + (num2 << 4) + 792] = 2;
        }
        for (int num16 = num3 - 1; num16 > _dungeonInfo[floor][address3]; num16--)
        {
            _dungeonInfo[floor][num16 + (num4 << 4) + 792] = 2;
        }
        if (num2 < num4)
        {
            for (int num17 = num2; num17 < num4 + 1; num17++)
            {
                _dungeonInfo[floor][(num17 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
            }
        }
        else if (num2 > num4)
        {
            for (int num18 = num4; num18 < num2 + 1; num18++)
            {
                _dungeonInfo[floor][(num18 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
            }
        }
        if (value3 == 0)
        {
            return true;
        }
        if (num2 < num4)
        {
            for (int num19 = num4 + 1; num19 < 16; num19++)
            {
                int num20 = _dungeonInfo[floor][(num19 << 4) + 792 + _dungeonInfo[floor][address3]];
                if (num20 != 1 && num20 != 3)
                {
                    for (int num21 = num4 + 1; num21 < num19; num21++)
                    {
                        _dungeonInfo[floor][(num21 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
                    }
                    break;
                }
            }
            for (int num22 = num2 - 1; num22 >= 0; num22--)
            {
                int num23 = _dungeonInfo[floor][(num22 << 4) + 792 + _dungeonInfo[floor][address3]];
                if (num23 != 1 && num23 != 3)
                {
                    for (int num24 = num2 - 1; num24 > num22; num24--)
                    {
                        _dungeonInfo[floor][(num24 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
                    }
                }
            }
        }
        else
        {
            for (int num25 = ((num2 >= num4) ? (num2 + 1) : num2); num25 < 16; num25++)
            {
                int num26 = _dungeonInfo[floor][(num25 << 4) + 792 + _dungeonInfo[floor][address3]];
                if (num26 != 1 && num26 != 3)
                {
                    for (int num27 = num2 + 1; num27 < num25; num27++)
                    {
                        _dungeonInfo[floor][(num27 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
                    }
                    break;
                }
            }
            for (int num25 = num4 - 1; num25 >= 0; num25--)
            {
                int num28 = _dungeonInfo[floor][(num25 << 4) + 792 + _dungeonInfo[floor][address3]];
                if (num28 != 1 && num28 != 3)
                {
                    for (int num29 = num4 - 1; num29 > num25; num29--)
                    {
                        _dungeonInfo[floor][(num29 << 4) + 792 + _dungeonInfo[floor][address3]] = 2;
                    }
                    break;
                }
            }
        }
    }
    return true;
}

uint MainWindow::routineI(int floor, uint value1, uint value2, uint value3)
{
    int num = _dungeonInfo[floor][value1 + (value2 << 4) + 792];
    if (num == 1 || num == 3)
    {
        return 255u;
    }
    int num2 = _dungeonInfo[floor][value1 + ((value2 - 1) << 4) + 792];
    int num3 = _dungeonInfo[floor][value1 + ((value2 - 1) << 4) + 792 - 1];
    int num4 = _dungeonInfo[floor][value1 + ((value2 - 1) << 4) + 792 + 1];
    int num5 = _dungeonInfo[floor][value1 + (value2 << 4) + 792 + 1];
    int num6 = _dungeonInfo[floor][value1 + ((value2 + 1) << 4) + 792 + 1];
    int num7 = _dungeonInfo[floor][value1 + ((value2 + 1) << 4) + 792];
    int num8 = _dungeonInfo[floor][value1 + ((value2 + 1) << 4) + 792 - 1];
    int num9 = _dungeonInfo[floor][value1 + (value2 << 4) + 792 - 1];
    if (num != 0 && num != 2)
    {
        num = (num + 252) & 0xFF;
        if (num > 4)
        {
            return value3;
        }
    }
    value3 = ((num3 != 1 && num3 != 3) ? (value3 & 0x7Fu) : (value3 | 0x80u));
    value3 = ((num4 != 1 && num4 != 3) ? (value3 & 0xDFu) : (value3 | 0x20u));
    value3 = ((num6 != 1 && num6 != 3) ? (value3 & 0xF7u) : (value3 | 8u));
    value3 = ((num8 != 1 && num8 != 3) ? (value3 & 0xFDu) : (value3 | 2u));
    value3 = ((num2 != 1 && num2 != 3) ? (value3 & 0xBFu) : (value3 | 0xE0u));
    value3 = ((num5 != 1 && num5 != 3) ? (value3 & 0xEFu) : (value3 | 0x38u));
    value3 = ((num7 != 1 && num7 != 3) ? (value3 & 0xFBu) : (value3 | 0xEu));
    value3 = ((num9 != 1 && num9 != 3) ? (value3 & 0xFEu) : (value3 | 0x83u));
    return value3;
}

void MainWindow::routineJ(int floor)
{
    int num = 0;
    int num2 = 0;
    int num3 = 0;
    int num4 = 0;
    int num5 = 0;
    int num6 = 0;
    while (true)
    {
        num3 = (int)GenerateRandom(0u, (uint)(_dungeonInfo[floor][23] - 1));
        num4 = num3 * 20 + 472;
        num5 = (int)GenerateRandom(_dungeonInfo[floor][num4], _dungeonInfo[floor][num4 + 2]);
        num6 = (int)GenerateRandom(_dungeonInfo[floor][num4 + 1], _dungeonInfo[floor][num4 + 3]);
        int num7 = 0;
        int num8 = 0;
        int num9 = 0;
        int num10 = 0;
        if (num2 < 100)
        {
            num7 = _dungeonInfo[floor][((num6 - 1) << 4) + num5 + 792];
            num8 = _dungeonInfo[floor][((num6 + 1) << 4) + num5 + 792];
            num9 = _dungeonInfo[floor][(num6 << 4) + 792 + num5 - 1];
            num10 = _dungeonInfo[floor][(num6 << 4) + 792 + num5 + 1];
            num7 = ((num7 == 1 || num7 == 3) ? 1 : 0);
            num8 = ((num8 == 1 || num8 == 3) ? 1 : 0);
            num9 = ((num9 == 1 || num9 == 3) ? 1 : 0);
            num10 = ((num10 == 1 || num10 == 3) ? 1 : 0);
            if (num7 != 0 && num8 != 0 && num9 != 0 && num10 != 0)
            {
                num2++;
                continue;
            }
            if (num7 != 0 && num10 != 0)
            {
                num2++;
                continue;
            }
            if (num7 != 0 && num9 != 0)
            {
                num2++;
                continue;
            }
            if (num8 != 0 && num10 != 0)
            {
                num2++;
                continue;
            }
            if (num8 != 0 && num9 != 0)
            {
                num2++;
                continue;
            }
            int num11 = (int)routineI(floor, (uint)num5, (uint)num6, 0u);
            int num12 = num11 & 0xFF;
            if (num12 == 234 || num12 == 171 || num12 == 174 || num12 == 186 || num12 == 163 || num12 == 142 || num12 == 58 || num12 == 232 || num12 == 184 || num12 == 226 || num12 == 139 || num12 == 46)
            {
                num2++;
                continue;
            }
        }
        _dungeonInfo[floor][num5 + (num6 << 4) + 792] = 4;
        _dungeonInfo[floor][4] = (uchar)num5;
        _dungeonInfo[floor][5] = (uchar)num6;
        num8 = num5;
        num9 = num6;
        while (true)
        {
            num6 = (int)GenerateRandom(0u, (uint)(_dungeonInfo[floor][23] - 1));
            if (num6 == (num3 & 0xFF) && (num & 0xFF) < 25)
            {
                num++;
                continue;
            }
            num4 = num6 * 20 + 472;
            num5 = (int)GenerateRandom(_dungeonInfo[floor][num4], _dungeonInfo[floor][num4 + 2]);
            num7 = num6;
            num6 = (int)GenerateRandom(_dungeonInfo[floor][num4 + 1], _dungeonInfo[floor][num4 + 3]);
            if ((num3 & 0xFF) != num7 || num5 != num8 || num6 != num9)
            {
                break;
            }
        }
        num7 = _dungeonInfo[floor][num5 + ((num6 - 1) << 4) + 792];
        num8 = _dungeonInfo[floor][num5 + ((num6 + 1) << 4) + 792];
        num9 = _dungeonInfo[floor][num5 + (num6 << 4) + 792 - 1];
        num10 = _dungeonInfo[floor][num5 + (num6 << 4) + 792 + 1];
        num7 = ((num7 == 1 || num7 == 3) ? 1 : 0);
        num8 = ((num8 == 1 || num8 == 3) ? 1 : 0);
        num9 = ((num9 == 1 || num9 == 3) ? 1 : 0);
        num10 = ((num10 == 1 || num10 == 3) ? 1 : 0);
        if (num7 != 0 && num8 != 0 && num9 != 0 && num10 != 0)
        {
            num2++;
            continue;
        }
        if (num7 != 0 && num10 != 0)
        {
            num2++;
            continue;
        }
        if (num7 != 0 && num9 != 0)
        {
            num2++;
            continue;
        }
        if (num8 != 0 && num10 != 0)
        {
            num2++;
            continue;
        }
        if (num8 == 0 || num9 == 0)
        {
            break;
        }
        num2++;
    }
    _dungeonInfo[floor][num5 + (num6 << 4) + 792] = 5;
    _dungeonInfo[floor][6] = (uchar)num5;
    _dungeonInfo[floor][7] = (uchar)num6;
}

int MainWindow::routineK(int floor)
{
    int num = (int)GenerateRandom(1u, 3u);
    _dungeonInfo[floor][8] = (uchar)num;
    int num2 = 0;
    int num3 = 0;
    while (true)
    {
        int num4 = (int)(GenerateRandom(0u, (uint)(_dungeonInfo[floor][23] - 1)) * 20 + 472);
        int num5 = (int)GenerateRandom(_dungeonInfo[floor][num4], _dungeonInfo[floor][num4 + 2]);
        int num6 = (int)GenerateRandom(_dungeonInfo[floor][num4 + 1], _dungeonInfo[floor][num4 + 3]);
        int num7 = _dungeonInfo[floor][num5 + ((num6 & 0xFF) << 4) + 792];
        if (num2 < 100 && (_dungeonInfo[floor][0] == 3 || _dungeonInfo[floor][0] == 1))
        {
            num2++;
            continue;
        }
        if (num7 == 6 || num7 == 4 || num7 == 5)
        {
            num2++;
            continue;
        }
        _dungeonInfo[floor][num5 + ((num6 & 0xFF) << 4) + 792] = 6;
        _dungeonInfo[floor][num3 * 2 + 13] = (uchar)num5;
        _dungeonInfo[floor][num3 * 2 + 14] = (uchar)num6;
        num3++;
        if (num3 >= (num & 0xFF))
        {
            break;
        }
    }
    return 1;
}

void MainWindow::GenerateFloorMap(int floor, uint address)
{
    int num;
    uchar* ptr = &_dungeonInfo[floor][address + 8];
    num = *(int*)ptr;
    int value = (_dungeonInfo[floor][num + 2] - _dungeonInfo[floor][num] + 1) / 2;
    int value2 = (_dungeonInfo[floor][num + 3] - _dungeonInfo[floor][num + 1] + 1) / 2;
    int num2 = 0;
    if (_dungeonInfo[floor][1] == 0 && GenerateRandom(0u, 15u) == 0)
    {
        num2 = 1;
    }
    int value3;
    int value4;
    int value5;
    int value6;
    if (num2 == 0)
    {
        value3 = _dungeonInfo[floor][address + 3] - _dungeonInfo[floor][num + 3];
        value4 = _dungeonInfo[floor][num] - _dungeonInfo[floor][address];
        value5 = _dungeonInfo[floor][address + 2] - _dungeonInfo[floor][num + 2];
        value6 = _dungeonInfo[floor][num + 1] - _dungeonInfo[floor][address + 1];
    }
    else
    {
        value3 = _dungeonInfo[floor][3] - _dungeonInfo[floor][num + 3] - 1;
        value4 = _dungeonInfo[floor][num] - 1;
        value5 = _dungeonInfo[floor][2] - _dungeonInfo[floor][num + 2] - 1;
        value6 = _dungeonInfo[floor][num + 1] - 1;
        _dungeonInfo[floor][1] = 1;
    }
    for (int i = _dungeonInfo[floor][num]; i <= _dungeonInfo[floor][num + 2]; i++)
    {
        int num3 = _dungeonInfo[floor][i + (_dungeonInfo[floor][num + 1] << 4) + 792];
        if (num3 == 1 || num3 == 3)
        {
            continue;
        }
        if (GenerateRandom(0u, 1u) == 0)
        {
            if (num3 == 8)
            {
                continue;
            }
            int num4 = _dungeonInfo[floor][i + ((_dungeonInfo[floor][num + 1] - 1) << 4) + 792];
            if (num4 != 1 && num4 != 8)
            {
                num3 = (int)GenerateRandom(0u, (uint)value2);
                int num5 = i - 1;
                for (int j = 0; j < num3 && _dungeonInfo[floor][i + ((_dungeonInfo[floor][num + 1] + j) << 4) + 792] != 8 && _dungeonInfo[floor][i + ((_dungeonInfo[floor][num + 1] + j + 1) << 4) + 792] != 1 && (_dungeonInfo[floor][((_dungeonInfo[floor][num + 1] + j) << 4) + i + 1 + 792] == 1 || _dungeonInfo[floor][((_dungeonInfo[floor][num + 1] + j + 1) << 4) + i + 1 + 792] != 1) && (_dungeonInfo[floor][((_dungeonInfo[floor][num + 1] + j) << 4) + num5 + 792] == 1 || _dungeonInfo[floor][num5 + ((_dungeonInfo[floor][num + 1] + j + 1) << 4) + 792] != 1); j++)
                {
                    _dungeonInfo[floor][i + ((_dungeonInfo[floor][num + 1] + j) << 4) + 792] = 1;
                }
            }
            continue;
        }
        num3 = (int)GenerateRandom(0u, (uint)value6);
        for (int k = 0; k < num3; k++)
        {
            int num6 = i + ((_dungeonInfo[floor][num + 1] - 1 - k) << 4) + 792;
            if (_dungeonInfo[floor][num6] != 8)
            {
                _dungeonInfo[floor][num6] = 0;
            }
        }
    }
    for (int l = _dungeonInfo[floor][num]; l <= _dungeonInfo[floor][num + 2]; l++)
    {
        value6 = _dungeonInfo[floor][l + (_dungeonInfo[floor][num + 3] << 4) + 792];
        if (value6 == 1 || value6 == 3)
        {
            continue;
        }
        if (GenerateRandom(0u, 1u) != 0)
        {
            value6 = (int)GenerateRandom(0u, (uint)value3);
            for (int m = 0; m < value6; m++)
            {
                if (_dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] + m + 1) << 4) + 792] != 8)
                {
                        _dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] + m + 1) << 4) + 792] = 0;
                }
            }
        }
        else
        {
            if (value6 == 8)
            {
                continue;
            }
            int num7 = _dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] + 1) << 4) + 792];
            if (num7 != 1 && num7 != 8)
            {
                int num8 = (int)GenerateRandom(0u, (uint)value2);
                value6 = l - 1;
                for (int n = 0; n < num8 && _dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] - n) << 4) + 792] != 8 && _dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] - n - 1) << 4) + 792] != 1 && (_dungeonInfo[floor][((_dungeonInfo[floor][num + 3] - n) << 4) + l + 1 + 792] == 1 || _dungeonInfo[floor][((_dungeonInfo[floor][num + 3] - n - 1) << 4) + l + 1 + 792] != 1) && (_dungeonInfo[floor][value6 + ((_dungeonInfo[floor][num + 3] - n) << 4) + 792] == 1 || _dungeonInfo[floor][value6 + ((_dungeonInfo[floor][num + 3] - n - 1) << 4) + 792] != 1); n++)
                {
                    _dungeonInfo[floor][l + ((_dungeonInfo[floor][num + 3] - n) << 4) + 792] = 1;
                }
            }
        }
    }
    for (int num9 = _dungeonInfo[floor][num + 1]; num9 <= _dungeonInfo[floor][num + 3]; num9++)
    {
        value6 = _dungeonInfo[floor][(num9 << 4) + 792 + _dungeonInfo[floor][num]];
        if (value6 == 1 || value6 == 3)
        {
            continue;
        }
        if (GenerateRandom(0u, 1u) != 0)
        {
            value6 = (int)GenerateRandom(0u, (uint)value4);
            for (int num10 = 0; num10 < value6; num10++)
            {
                if (_dungeonInfo[floor][(num9 << 4) + 792 + _dungeonInfo[floor][num] - 1 - num10] != 8)
                {
                    _dungeonInfo[floor][(num9 << 4) + 792 + _dungeonInfo[floor][num] - 1 - num10] = 0;
                }
            }
        }
        else
        {
            if (value6 == 8)
            {
                continue;
            }
            int num11 = _dungeonInfo[floor][(num9 << 4) + 792 + _dungeonInfo[floor][num] - 1];
            if (num11 != 1 && num11 != 8)
            {
                int num12 = (int)GenerateRandom(0u, (uint)value);
                value6 = num9 - 1;
                for (int num13 = 0; num13 < num12 && _dungeonInfo[floor][num13 + (num9 << 4) + _dungeonInfo[floor][num] + 792] != 8 && _dungeonInfo[floor][num13 + (num9 << 4) + _dungeonInfo[floor][num] + 792 + 1] != 1 && (_dungeonInfo[floor][num13 + ((num9 + 1) << 4) + _dungeonInfo[floor][num] + 792] == 1 || _dungeonInfo[floor][num13 + ((num9 + 1) << 4) + _dungeonInfo[floor][num] + 792 + 1] != 1) && (_dungeonInfo[floor][num13 + (value6 << 4) + _dungeonInfo[floor][num] + 792] == 1 || _dungeonInfo[floor][num13 + (value6 << 4) + _dungeonInfo[floor][num] + 792 + 1] != 1); num13++)
                {
                    _dungeonInfo[floor][num13 + (num9 << 4) + _dungeonInfo[floor][num] + 792] = 1;
                }
            }
        }
    }
    for (int num14 = _dungeonInfo[floor][num + 1]; num14 <= _dungeonInfo[floor][num + 3]; num14++)
    {
        value6 = _dungeonInfo[floor][(num14 << 4) + 792 + _dungeonInfo[floor][num + 2]];
        if (value6 == 1 || value6 == 3)
        {
            continue;
        }
        if (GenerateRandom(0u, 1u) != 0)
        {
            value6 = (int)GenerateRandom(0u, (uint)value5);
            for (int num15 = 0; num15 < value6; num15++)
            {
                if (_dungeonInfo[floor][num15 + (num14 << 4) + _dungeonInfo[floor][num + 2] + 792 + 1] != 8)
                {
                        _dungeonInfo[floor][num15 + (num14 << 4) + _dungeonInfo[floor][num + 2] + 792 + 1] = 0;
                }
            }
        }
        else
        {
            if (value6 == 8)
            {
                continue;
            }
            int num16 = _dungeonInfo[floor][_dungeonInfo[floor][num + 1] + (num14 << 4) + 792 + 1];
            if (num16 != 1 && num16 != 8)
            {
                int num17 = (int)GenerateRandom(0u, (uint)value);
                value6 = num14 - 1;
                for (int num18 = 0; num18 < num17 && _dungeonInfo[floor][(num14 << 4) + 792 + _dungeonInfo[floor][num + 2] - num18] != 8 && _dungeonInfo[floor][(num14 << 4) + 792 + _dungeonInfo[floor][num + 2] - num18 - 1] != 1 && (_dungeonInfo[floor][((num14 + 1) << 4) + 792 + _dungeonInfo[floor][num + 2] - num18] == 1 || _dungeonInfo[floor][((num14 + 1) << 4) + 792 + _dungeonInfo[floor][num + 2] - num18 - 1] != 1) && (_dungeonInfo[floor][(value6 << 4) + 792 + _dungeonInfo[floor][num + 2] - num18] == 1 || _dungeonInfo[floor][(value6 << 4) + 792 + _dungeonInfo[floor][num + 2] - num18 - 1] != 1); num18++)
                {
                    _dungeonInfo[floor][(num14 << 4) + 792 + _dungeonInfo[floor][num + 2] - num18] = 1;
                }
            }
        }
    }
}

uint MainWindow::GetItemRank(uint value1, uint value2)
{
    int num = (int)GenerateRandom();
    float num2 = num;
    num2 -= 1.f;
    int num3 = (int)(value2 - value1 + 1);
    num2 = (float)num3 * num2 / 32767.f;
    return (uint)num2 + value1;
}

uchar MainWindow::FloorCount()
{
    return _details[1];
}

uchar MainWindow::BossIndex()
{
    return _details[0]-1;
}

uchar MainWindow::MonsterRank()
{
    return _details[2];
}

uchar MainWindow::MapTypeIndex()
{
    return _details[3]-1;
}

QByteArray MainWindow::GetFloorMap(int floor)
{
    if (floor < FloorCount())
    {
        int floorHeight = GetFloorHeight(floor);
        int floorWidth = GetFloorWidth(floor);
        QByteArray array;
        array.clear();
        //int num = floor * 1336 + 792;
        int num = 792;
        //int num2 = 0;
        for (int i = 0; i < floorHeight; i++)
        {
            for (int j = 0; j < floorWidth; j++)
            {
                array.append(_dungeonInfo[floor][num+j]);
            }
            num += 16;
        }
        return array;
    }
    QByteArray array2;
    return array2;
}

int MainWindow::GetFloorHeight(int floor)
{
    if (floor < FloorCount())
    {
        return _dungeonInfo[floor][3];
    }
    return 0;
}

int MainWindow::GetFloorWidth(int floor)
{
    if (floor < FloorCount())
    {
        return _dungeonInfo[floor][2];
    }
    return 0;
}

bool MainWindow::IsUpStep(int floor, int x, int y)
{
    if (floor < FloorCount() && x < GetFloorWidth(floor) && y < GetFloorHeight(floor))
    {
        if (_dungeonInfo[floor][4] == x)
        {
            return _dungeonInfo[floor][5] == y;
        }
        return false;
    }
    return false;
}

int MainWindow::GetTreasureBoxIndex(int floor, int x, int y)
{
    if (floor < FloorCount() && x < GetFloorWidth(floor) && y < GetFloorHeight(floor))
    {
        for (int i = 0; i < GetTreasureBoxCountPerFloor(floor); i++)
        {
            if (_dungeonInfo[floor][i * 2 + 13] == x && _dungeonInfo[floor][i * 2 + 14] == y)
            {
                return i;
            }
        }
    }
    return -1;
}

int MainWindow::GetTreasureBoxCountPerFloor(int floor)
{
    if (floor < FloorCount())
    {
        return _dungeonInfo[floor][8];
    }
    return 0;
}

int MainWindow::GetTreasureBoxRankPerFloor(int floor, int index)
{
    if (floor < FloorCount() && index < GetTreasureBoxCountPerFloor(floor))
    {
        return _dungeonInfo[floor][9 + index];
    }
    return 0;
}

uint MainWindow::routineRandom(uint value)
{
    int num = (int)GenerateRandom();
    float num2 = (float)num - 1.f;
    return (uint)(num2 * (float)value / 32767.f);
}

QString MainWindow::GetTreasureBoxItem(int floor, int boxIndex, int second)
{
    _seed = (uint)(_dungeonInfo[floor][0] + MapSeed + second - 5);
    for (int i = 0; i < _dungeonInfo[floor][8]; i++)
    {
        int num = (int)routineRandom(100u);
        if (i != boxIndex)
        {
            continue;
        }
        int num2 = _dungeonInfo[floor][i + 9];
        int num3 = TableO[num2 - 1];
        int num4 = TableO[num2];
        int num5 = 0;
        for (int j = num3; j < num4; j++)
        {
            num5 += TableP[j];
            if (num < num5)
            {
                return TableR[TableQ[j]];
            }
        }
    }
    return NULL;
}

//###############################################################################################################################
//###############################                     Function Hoimi Table                       ################################
//###############################################################################################################################

double MainWindow::Hoimi_min(int c)
{
    float hoimi_min=(c-0.5)/span*0x100000000;
    return hoimi_min;
}

double MainWindow::Hoimi_max(int c)
{
    float hoimi_max=(c+0.5)/span*0x100000000;
    return hoimi_max;
}

void MainWindow::Hoimi_rand()
{
    unsigned int sd_tmp0, sd_tmp1, sd_tmp2, sd_tmp3, tmp0, tmp1, tmp2, tmp3, carry;

    tmp0 = (seed0 * 0x8965);
    sd_tmp0 = tmp0 & 0xffff;
    carry = tmp0 >> 16;

    tmp2 = carry & 0xffff;
    carry = carry >> 16;
    tmp0 = (seed0 * 0x6c07);
    carry += tmp0 >> 16;
    tmp0 = tmp0 & 0xffff;
    tmp1 = (seed1 * 0x8965);
    carry += tmp1 >> 16;
    tmp1 = tmp1 & 0xffff;
    tmp2 += tmp1 + tmp0;
    carry += tmp2 >> 16;
    sd_tmp1 = tmp2 & 0xffff;

    tmp3 = carry & 0xffff;
    carry = carry >> 16;
    tmp0 = (seed0 * 0x8b65);
    carry += tmp0 >> 16;
    tmp0 = tmp0 & 0xffff;
    tmp1 = (seed1 * 0x6c07);
    carry += tmp1 >> 16;
    tmp1 = tmp1 & 0xffff;
    tmp2 = (seed2 * 0x8965);
    carry += tmp2 >> 16;
    tmp2 = tmp2 & 0xffff;
    tmp3 += tmp2 + tmp1 + tmp0;
    carry += tmp3 >> 16;
    sd_tmp2 = tmp3 & 0xffff;

    sd_tmp3 = ((carry & 0xffff) + ((seed3 * 0x8965) & 0xffff) + ((seed2 * 0x6c07) & 0xffff) + ((seed1 * 0x8b65) & 0xffff) + ((seed0 * 0x5d58) & 0xffff)) & 0xffff;

    tmp0 = sd_tmp0 + 0x9ec3;
    sd_tmp0 = tmp0 & 0xffff;
    carry = tmp0 >> 16;

    tmp0 = sd_tmp1 + 0x26 + carry;
    sd_tmp1 = tmp0 & 0xffff;
    carry = tmp0 >> 16;

    if (carry > 0) {
        tmp0 = sd_tmp2 + carry;
        sd_tmp2 = tmp0 & 0xffff;
        carry = tmp0 >> 16;

        tmp0 = sd_tmp3 + carry;
        sd_tmp3 = tmp0 & 0xffff;
    }
    seed0 = sd_tmp0;
    seed1 = sd_tmp1;
    seed2 = sd_tmp2;
    seed3 = sd_tmp3;
}

void MainWindow::Filltable()
{
    Filltable(0);
}

void MainWindow::Filltable(int n)
{
    //Gestion des couleurs
    QList<QSpinBox*> childsb;
    childsb.append(HoimiUi->findChild<QSpinBox*>("Pink"));
    childsb.append(HoimiUi->findChild<QSpinBox*>("Blue"));

    //Creation de la liste des cases du tableau des valeurs
    QList<QLabel*> childlbl;
    for (int i=0;i<11; i++)
    {
        for (int j=0; j<10; j++)
        {
            childlbl.append(HoimiUi->findChild<QLabel*>(QString::number(i*10+j)));
        }
    }

    //Initialisation des valeurs
    int sd=seed;
    int setpink=childsb[0]->value();
    int setblue=childsb[1]->value();
    seed0=sd & 0xffff;
    seed1=sd>>16;
    seed2=0;
    seed3=0;

    if(n!=0){
        for (int i=0;i<100*n; i++) {Hoimi_rand();}
        for (int i=0;i<11; i++)
        {
            HoimiUi->findChild<QLabel*>("R"+QString::number(i))->setText(QString::number((i*10)+100*n));
        }
    }

    for (int r=0;r<11; r++) {
        for (int c=0; c<10; c++) {
            QString txt="";
            Hoimi_rand();
            double d = ((double(seed3)*0x10000+double(seed2))/0x100000000)*10000;
            int p100 = d / 100;
            txt=QString::number(p100)+","+QString::number(int(d-100*p100));

            childlbl[r*10+c]->setText(txt);

            if (p100<setpink) {childlbl[r*10+c]->setStyleSheet("background-color:#ffaaff;""border: 1px solid black;");}
            else if (p100<setblue) {childlbl[r*10+c]->setStyleSheet("background-color:#65aaff;""border: 1px solid black;");}
            else {childlbl[r*10+c]->setStyleSheet("background-color:#ededed;""border: 1px solid black;");}
        }
    }

    childlbl[posH]->setStyleSheet("background-color:#dada20;");
}

void MainWindow::Table_Color()
{
    QList<QSpinBox*> childsb;
    childsb.append(HoimiUi->findChild<QSpinBox*>("Pink"));
    childsb.append(HoimiUi->findChild<QSpinBox*>("Blue"));

    int setpink=childsb[0]->value();
    int setblue=childsb[1]->value();

    QList<QLabel*> childlbl;
    childlbl.append(HoimiUi->findChild<QLabel*>(QString::number(posH)));
    childlbl.append(HoimiUi->findChild<QLabel*>(QString::number(Last_posH)));

    QString txt = childlbl[1]->text();
    int val;
    if (txt.left(2).toInt()<txt.left(1).toInt()) {val = txt.left(1).toInt();}
    else {val = txt.left(2).toInt();}

    if (val<setpink) {childlbl[1]->setStyleSheet("background-color:#ffaaff;""border: 1px solid black;");}
    else if (val<setblue) {childlbl[1]->setStyleSheet("background-color:#65aaff;""border: 1px solid black;");}
    else {childlbl[1]->setStyleSheet("background-color:#ededed;""border: 1px solid black;");}

    childlbl[0]->setStyleSheet("background-color:#dada20;");

    /*
    int val2;
    if (txt.right(2).toInt()<txt.right(1).toInt())
        val2 = txt.right(1).toInt();
    else
        val2 = txt.right(2).toInt();

    qDebug() << val << val2;*/

}

void MainWindow::Deplacement(int x)
{
    Last_posH=posH;
    posH+=x;
    if (posH>109){
        posH=posH-100;
        posG+=1;
        Filltable(posG);
    }
    QLabel* lbl = HoimiUi->findChild<QLabel*>("P");
    lbl->setText(QString::number(posH+(100*posG)));
    Table_Color();
}

void MainWindow::Reset_Window()
{
    qDeleteAll(HoimiUi->findChildren<QPushButton*>());
    qDeleteAll(HoimiUi->findChildren<QRadioButton*>());
    qDeleteAll(HoimiUi->findChildren<QLabel*>());
    qDeleteAll(HoimiUi->findChildren<QSpinBox*>());
    qDeleteAll(HoimiUi->findChildren<QLineEdit*>());
}

void MainWindow::Table()
{
    for (int i=0; i<12; i++)
    {
        for (int j=0; j<11; j++)
        {
            QLabel* lbl = new QLabel(HoimiUi);
            lbl->setGeometry(j*35+20,i*35+120,36,36);
            lbl->setStyleSheet("background-color:#ededed;""border: 1px solid black;");
            lbl->setAlignment(Qt::AlignCenter);
            if (i==0 && j==0){ goto jump;}
            else if (i==0) {lbl->setText(QString::number(j-1));}
            else if (j==0) {lbl->setText(QString::number((i-1)*10));lbl->setObjectName(QString("R")+QString::number(i-1));}
            else {lbl->setObjectName(QString::number((i-1)*10+j-1));}
            jump:;
            lbl->show();
        }
    }
}

void MainWindow::InitHoimiUi()
{
    HoimiUi = new QWidget(this,Qt::Popup|Qt::Dialog);
    HoimiUi->setWindowModality(Qt::ApplicationModal);
    HoimiUi->setWindowTitle("Table d'Hoimi");
    HoimiUi->setFixedSize(420,590);
    HoimiUi->move(100,100);
    HoimiUi->setStyleSheet("QSpinBox { background: #e7fffc }""QPushButton { background: #ff9092 }");
    HoimiUi->setPalette(QPalette(QPalette::Window,"#bdbfe8"));
    HoimiUi->show();

    baseheal=30;
    span=10;

    InitHoimi();
}

void MainWindow::InitHoimi()
{
    int y = 0;
    QList<char> Name = {'P','N','D'};
    QList<int> posH = {80, 180, 290};
    QString Name_pos;

    for (int i=0; i<8; i++)
    {
        Name_pos=QString::number(i+1);
        y=180+i*25;
        QLabel* lbl = new QLabel(HoimiUi);
        lbl->setGeometry(60,y,10,20);
        lbl->setText(Name_pos);
        //lbl->setFont(QFont ("Rockwell",10,QFont::Bold));
        lbl->show();
        for (int j=0; j<3; j++)
        {
            QSpinBox* sb = new QSpinBox(HoimiUi);
            if (j==2)
                sb->setGeometry(posH[j],y,70,20);
            else
                sb->setGeometry(posH[j],y,90,20);
            //sb->setFont(QFont ("Rockwell", 10,QFont::Bold));
            sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
            sb->setRange(0,999);
            sb->setObjectName(Name[j]+Name_pos);
            sb->setAlignment(Qt::AlignCenter);
            connect(sb, SIGNAL(valueChanged(int)), this, SLOT(Calculate()));
            sb->show();
        }
    }

    QList<QRect> Other_pos =
    {
        QRect(140,20,75,20),QRect(35,50,65,20),QRect(160,120,75,20),QRect(80,155,90,20),QRect(180,155,90,20),QRect(290,155,70,20),
        // 0 : Label 1, 1 : Label 2, 2 : Label 3, 3 : Label 4, 4 : Label 5, 5 : Label 6,
        QRect(140,80,140,30),QRect(120,390,80,20),QRect(220,390,80,20),QRect(150,430,120,20),QRect(150,550,120,30),QRect(390,10,20,20),
        // 6 : Soin, 7 : Calculate, 8 : Reset, 9 : Function, 10 : Quit, 11 : Help  => Bouton
        QRect(220,20,60,20),QRect(105,50,100,20),QRect(210,50,80,20),QRect(295,50,80,20),QRect(235,120,25,20),QRect(150,470,120,30),
        // 12 : Magical mending, 13 : Heal, 14 : Midheal, 15 : Moreheal, 16 : Base heal, 17 : Seed
        QRect(150,510,120,30)
        // 18 : Seed
    };

    QStringList Other_Text = {"Soin Magique","Sort Utilisé :","Soin de Base :","Précèdent PV","Nouveau PV","Différence",
                              "Calculer le soin de base","Calculer","Réinitialiser","","Quitter","",
                              "","Premier secours","Soin Partiel","Soin Avancé","30",""};

    for (int i=0; i<6; i++)
    {
        QLabel* lbl = new QLabel(HoimiUi);
        lbl->setGeometry(Other_pos[i]);
        lbl->setText(Other_Text[i]);
        lbl->setAlignment(Qt::AlignCenter);
        //lbl->setFont(QFont ("Rockwell",10,QFont::Bold));
        lbl->show();

        QPushButton* pb = new QPushButton(HoimiUi);
        pb->setGeometry(Other_pos[i+6]);
        pb->setText(Other_Text[i+6]);
        switch (i)
        {
            case 0:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Base())); break; }
            case 1:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Calculate())); break; }
            case 2:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Reset())); break; }
            case 3:
            {
                pb->setFlat(true);
                pb->setStyleSheet("background-color:#bdbfe8;");
                pb->setObjectName("F");
                pb->setEnabled(false);
                connect(pb, SIGNAL(clicked()),this, SLOT(Function()));
                break;
            }
            case 4:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Quit())); break; }
            case 5:
            {
                pb->setIcon(QIcon(":/icon.png"));
                pb->setFlat(true);
                pb->setStyleSheet("background-color:#bdbfe8;");
                pb->setIconSize(QSize(20,20));
                connect(pb, SIGNAL(clicked()),this, SLOT(Help()));
                break;
            }
            default:
                break;
        }
        pb->show();

    }

    QSpinBox* sb = new QSpinBox(HoimiUi);
    sb->setGeometry(Other_pos[12]);
    //sb->setFont(QFont ("Rockwell", 10,QFont::Bold));
    sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sb->setRange(0,999);
    sb->setObjectName("M");
    sb->setAlignment(Qt::AlignCenter);
    connect(sb, SIGNAL(valueChanged(int)), this, SLOT(Base()));
    sb->show();

    for (int i=0; i<3; i++)
    {
        QRadioButton* rb = new QRadioButton(HoimiUi);
        rb->setGeometry(Other_pos[i+13]);
        rb->setFont(QFont ("Segoe UI", 7));
        rb->setText(Other_Text[i+13]);
        rb->setObjectName("rb"+QString::number(i));
        if (i==0)
            rb->setChecked(true);
        connect(rb, SIGNAL(clicked()), this, SLOT(Base()));
        rb->show();
    }

    QLabel* lbl = new QLabel(HoimiUi);
    lbl->setGeometry(Other_pos[16]);
    lbl->setObjectName("B");
    lbl->setText(Other_Text[16]);
    //lbl->setFont(QFont ("Rockwell",10,QFont::Bold));
    lbl->show();

    QLineEdit* le = new QLineEdit(HoimiUi);
    le->setGeometry(Other_pos[17]);
    le->setObjectName("Seed");
    //le->setText(Other_Text[16]);
    //le->setFont(QFont ("Rockwell",10,QFont::Bold));
    le->show();

    QPushButton* ss = new QPushButton(HoimiUi);
    ss->setGeometry(Other_pos[18]);
    //ss->setObjectName("SS");
    ss->setText("Chercher par seed");
    //ss->setFont(QFont ("Rockwell",10,QFont::Bold));
    connect(ss, SIGNAL(clicked()),this, SLOT(SearchSeed()));
    ss->show();
}

void MainWindow::SearchSeed()
{
    QLineEdit* le = HoimiUi->findChild<QLineEdit*>("Seed");
    bool* ok = nullptr;
    seed=0;
    seed=le->text().toInt(ok,16);
    //qDebug() << seed;
    posH=0;
    Reset();
    Function();
}

void MainWindow::Reset()
{
    QList<char> Name = {'P','N','D'};
    QList<QSpinBox*> childsb;
    for (int i=1; i<9; i++)
    {
        for (int j=0; j<3; j++)
        {
            childsb.append(HoimiUi->findChild<QSpinBox*>(Name[j]+QString::number(i)));
        }
    }

    for (int i=0; i<24; i++)
    {
        childsb[i]->setValue(0);
    }

    QPushButton* pb = HoimiUi->findChild<QPushButton*>("F");
    pb->setText("");
}

void MainWindow::Calculate()
{
    QList<char> Name = {'P','N','D'};
    QList<QSpinBox*> childsb;
    for (int i=1; i<9; i++)
    {
        for (int j=0; j<3; j++)
        {
            childsb.append(HoimiUi->findChild<QSpinBox*>(Name[j]+QString::number(i)));
        }
    }

    QPushButton* pb = HoimiUi->findChild<QPushButton*>("F");

    int val;

    for (int i=0; i<8; i++)
    {
        val = childsb[(i*3)+1]->value()-childsb[i*3]->value();
        if(val>0)
        {
            childsb[(i*3)+2]->setValue(val);
        }
        else
        {
            //childsb[(i*3)+1]->setValue(childsb[i*3]->value());
        }
    }

    for (int i=0; i<7; i++)
    {
        val = childsb[(i*3)+1]->value()-childsb[i*3]->value()-childsb[(i*3)+2]->value();
        if(childsb[(i*3)+1]->value()>0 && childsb[(i*3)+2]->value()>30 && val==0)
        {
            childsb[(i*3)+3]->setValue(childsb[(i*3)+1]->value());
        }
    }

    posH=0;
    while(posH<8)
    {
        if(childsb[(posH*3)+2]->value()<30)
            break;
        posH++;
    }

    double Tab[]={2531011, 1437506738, 2697179901, 3373310356, 356160039, 730474278, 916809409, 1319924200, 1350350667, 645861978, 1566083941, 1915515864, 847946054, 895207022, 2905333808, 1837436634, 1454510125, 1817326800, 1359447427, 324470299, 1812433253, 88293849, 1790253981, 42330609, 3130934549, 318537289, 3711034317, 2922550497, 3385337285, 3700446137, 0, 1904791564, 183838931, 176901684, 3359619440, 205866298, 3425238665, 580285797, 952588127, 1736004865};//al mh ml ah
    double c;
    int Array[131072];
    int a=0;
    QString txt;

    for (int seed = 0x3ffff; seed>=0x20000; seed--)
    {
        for (int i=0; i<posH; i++)
        {
            c=fmod((((seed*Tab[20+i]+Tab[i])/4294967296)+seed*Tab[10+i]+Tab[30+i]),4294967296);
            if (c<0)
            {
                c=c+4294967296;
            }
            val=childsb[(i*3)+2]->value()-baseheal;
            if (c<Hoimi_min(val) || c>=Hoimi_max(val))
            {
                goto loop;
            }
        }
        Array[a]=seed;
        a++;
        loop:;
    }
    for (int i=0; i<a; i++)
    {
        txt+=(QString::number(Array[i],16).toUpper());
    }
    if (a==1)
    {
        seed=Array[0];
        pb->setEnabled(true);
        pb->setText(txt);
    }
    else
    {
        seed=0;
        pb->setEnabled(false);
        pb->setText("");
    }
}

void MainWindow::Base()
{
    // Récupération des QRadioButton dont les ObjectName correspondent a rb0, rb1 et rb2
    QList<QRadioButton*> childrb;
    QStringList namerb = {"rb0","rb1","rb2"};
    for (int i=0; i<3; i++)
    {
        childrb.append(HoimiUi->findChild<QRadioButton*>(namerb[i]));
    }

    // Récupération du QQSpinBox et Qlabel dont le ObjectName correspondent a M et B
    QSpinBox* sp = HoimiUi->findChild<QSpinBox*>("M");
    QLabel* lbl = HoimiUi->findChild<QLabel*>("B");

    // Calcul de la valeur de soin de base
    int a;
    int mending = sp->value();
    if (childrb[0]->isChecked())
    {
        span=10;
        a = mending-50;
        a = a<0 ? 0 : a;
        baseheal = 30+a*125/949;
    }
    else if (childrb[1]->isChecked())
    {
        span=20;
        a = mending-100;
        a = a<0 ? 0 : a;
        baseheal = 75+a*215/899;
    }
    else
    {
        span=40;
        a = mending-200;
        a = a<0 ? 0 : a;
        baseheal = 165+a*415/799;
    }

    // Affichage de la valeur de soin de base
    lbl->setText(QString::number(baseheal));
}

void MainWindow::Function()
{
    Reset_Window();
    posG=0;

    QStringList Text = {"Seed :","Soin de base :","position :","Rose :","%","Bleu :","%",
                        "Configurer","+1","+4","+8","Retour",""};


    QList<QRect> posP =
    {
        QRect(60,20,35,20),QRect(170,20,75,20),QRect(300,20,50,20),QRect(60,50,40,20),QRect(160,50,20,20),QRect(180,50,40,20),QRect(280,50,20,20),
        // label : 0->6
        QRect(300,50,60,20),QRect(75,80,80,25),QRect(170,80,80,25),QRect(265,80,80,25),QRect(150,550,120,30),QRect(390,10,20,20),
        // 7: Configure, 8: +1, 9: +4, 10: +8, 11: Return, 12: Help => Button
        QRect(95,20,35,20),QRect(245,20,20,20),QRect(350,20,40,20),QRect(100,50,60,20),QRect(220,50,60,20),QRect(0,0,0,0)
        // 13: Seed, 14: Baseheal, 15: position, 16: Pink, 17: Blue, 18:
    };

    for (int i=0; i<7; i++)
    {
        QLabel* lbl = new QLabel(HoimiUi);
        lbl->setGeometry(posP[i]);
        lbl->setText(Text[i]);
        lbl->show();
    }

    for (int i=0; i<6; i++)
    {
        QPushButton* pb = new QPushButton(HoimiUi);
        pb->setGeometry(posP[i+7]);
        pb->setText(Text[i+7]);
        switch (i)
        {
            case 0:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Filltable())); break; }
            case 1:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Plus1())); break; }
            case 2:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Plus4())); break; }
            case 3:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Plus8())); break; }
            case 4:
            { connect(pb, SIGNAL(clicked()),this, SLOT(Return_Page())); break; }
            case 5:
            {
                pb->setIcon(QIcon(":/icon.png"));
                pb->setFlat(true);
                pb->setStyleSheet("background-color:#bdbfe8;");
                pb->setIconSize(QSize(20,20));connect(pb, SIGNAL(clicked()),this, SLOT(Help2()));
                break;
            }
            default:
                break;
        }
        pb->show();
    }

    for (int i=0; i<3; i++)
    {
        QLabel* lbl = new QLabel(HoimiUi);
        lbl->setGeometry(posP[i+13]);
        switch (i)
        {
            case 0:
            { lbl->setText(QString::number(seed,16).toUpper()); break; }
            case 1:
            { lbl->setText(QString::number(baseheal)); break; }
            case 2:
        { lbl->setText(QString::number(posH)); lbl->setObjectName("P"); lbl->setAlignment(Qt::AlignVCenter|Qt::AlignLeft); break; }
            default:
                break;
        }
        lbl->show();
    }

    QSpinBox* sb = new QSpinBox(HoimiUi);
    sb->setGeometry(posP[16]);
    sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sb->setRange(0,100);
    sb->setObjectName("Pink");
    sb->setValue(2);
    sb->show();

    QSpinBox* sb2 = new QSpinBox(HoimiUi);
    sb2->setGeometry(posP[17]);
    sb2->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sb->setRange(0,100);
    sb2->setObjectName("Blue");
    sb2->setValue(10);
    sb2->show();

    Table();

    seed0=0;
    seed1=0;
    seed2=0;
    seed3=0;

    Filltable();
}

void MainWindow::Plus1()
{
    Deplacement(1);
}

void MainWindow::Plus4()
{
    Deplacement(4);
}

void MainWindow::Plus8()
{
    Deplacement(8);
}

void MainWindow::Quit()
{
    HoimiUi->close();
}

void MainWindow::Return_Page()
{
    Reset_Window();
    InitHoimi();
}

void MainWindow::Help()
{
    QMessageBox msgBox;
    msgBox.setText("1) Se rendre dans une antre\n\n"
                   "2) Se rendre au dernier étage de l\'antre\n\n"
                   "3) Faire une sauvegarde rapide du jeu\n\n"
                   "4) Éteindre la console\n\n"
                   "5) Rallumez la console et relancer votre jeu en chargeant la sauvegarde rapide\n"
                   "Allez dans le menu/caractérist.\n"
                   "Choisissez le perso qui va faire le sort Premier secours\n"
                   "Et regardez sa stat \"Soins magique\" : notez la valeur puis appuyer sur le bouton \"Calculer le soin de base\"\n\n"
                   "6) Notez les HP de départ du perso que vous allez soigner\n"
                   "Prenez votre soigneur (Celui pour lequel vous avez noté la stat)\n"
                   "Utilisez alors le sort \"Premier secours\", \"Soin Partiel\" ou \"Soin Avancé\", selon le choix effectué, sur le perso à soigner et notez alors à combien sont passés ses HP une fois que vous l'avez soigné\n"
                   "Répétez alors le soin jusqu'a qu'il est un lien qui apparait\n");
    msgBox.setWindowTitle("Aide");
    msgBox.setWindowIcon(QIcon(":/Dq9.ico"));
    msgBox.exec();
}

void MainWindow::Help2()
{
    QMessageBox msgBox;
    msgBox.setText("1) S'il n\'y a aucune case rose, il faut recommencer la procédure depuis le début\n\n"
                   "2) Le but va maintenant être de se placer sur une case rose ou bleu selon votre choix\n"
                   "Pour avancer il faut lancer un sort de soin\n"
                   "Tous les sorts de soins comptent pour une avance de 1 case (Bouton +1)\n"
                   "Le sort Multisoins compte pour une avance de 4 cases (Bouton +4)\n"
                   "Le sort Omnisoins compte pour une avance de 8 cases (Bouton +8)\n"
                   "On peut lancer les sort de soins même si tous les joueurs ont la vie pleine\n\n"
                   "3) Une fois fait, il ne vous reste plus qu\'à affronter le boss directement\n"
                   "Par contre pendant le combat vous pouvez faire ce que vous voulez (sorts, soins, attaque, aptitude, équipement, etc ...)\n"
                   "La seul restriction : ne pas utiliser de \"Coup de grâce\" pendant tout le combat\n");
    msgBox.setWindowTitle("Aide");
    msgBox.setWindowIcon(QIcon(":/Dq9.ico"));
    msgBox.exec();
}

//###############################################################################################################################
//###############################                     Function Monster Info                      ################################
//###############################################################################################################################

void MainWindow::InitMonsterUi()
{
    MonsterUi = new QWidget(this,Qt::Popup|Qt::Dialog);
    MonsterUi->setWindowModality(Qt::ApplicationModal);
    MonsterUi->setWindowTitle("Monstre & Cie");
    MonsterUi->setFixedSize(750,350);
    MonsterUi->move(100,100);
    MonsterUi->show();

    //BossWeak = {"Light", "Dark", "Dark", "Blast", "Earth", "Blast and Light", "Light", "Ice", "Fire",  "Ice", "Wind and Light", "Ice and Dark"};
    BossWeak = {"Lumière", "Ténèbres", "Ténèbres", "Explosion", "Terre", "Explosion et Lumière", "Lumière", "Glace", "Feu",  "Glace", "Vent et Lumière", "Glace et Ténèbres"};
    //BossDrop = {"Dragon Scale (steal)\n   Dragontail Whip (10%)\n   Vesta Gauntlets (2%)" ,"White tights (steal)\n   Fire claws (10%)\n   Invincible trousers (2%)", "Platinum headgear (steal)\n   Miracle sword (5%)\n   Apprentice's gloves (2%)" ,"Handrills (steal)\n   Pro's axe (5%)\n   Hero's boots (2%)", "Star's suit (steal)\n   Supernatural specs (5%)\n   Sensible sandals (2%)", "Crimson boots (steal)\n   Sacred armour (5%)\n   Brain drainer (2%)", "Tough guy tattoo (steal)\n   Giant's hammer (5%)\n   Tropotoga (2%)", "Aggressence (steal)\n   Warrior's armour (5%)\n   Hallowed helm (2%)", "Sage's elixir (steal)\n   Minerva's mitre (5%)\n   Spring breeze hat (2%)", "Fencing jacket (steal)\n   Tactical vest (5%)\n   Victorious armour (2%)", "Narpsicious (steal)\n   Gold bar (10%)\n   Angel's robe (2%)", "Yggdrasil leaf (steal)\n   Yggdrasil leaf (100%)\n   Orichalcum (5%)"};
    BossDrop = {"825 PO\n   Ecaille du dragon (vol)\n   Fouet du dragon (10%)\n   Gantelet de Vesta(2%)", "967 PO\n   Collants blancs (vol)\n   Griffes de feu (10%)\n   Pantalon d'invincibilité (2%)", "1110 PO\n   Heaume de platine (vol)\n   Epée miracle (5%)\n   Gants d'apprenti (2%)", "1252 PO\n   Vrillons (vol)\n   Hache de pro (5%)\n   Bottes de héros (2%)", "1395 PO\n   Habits de star (vol)\n   Lunettes surnaturelles (5%)\n   Spartiates adéquates (2%)", "1537 PO\n   BottesPO\n   urpres (vol)\n   Armure sacrée (5%)\n   Boucliéponge (2%)", "1680 PO\n   Tatouage de costaud (vol)\n   Marteau de géant (5%)\n   Tropotoge (2%)", "1822 PO\n   Agressence (vol)\n   Armure de guerrier (5%)\n   Heaume sacré (2%)", "1965 PO\n   Elixir du sage (vol)\n   Mitre de minerve (5%)\n   Coiffe printanière (2%)", "2107 PO\n   Veste d'escrime (vol)\n   Cuirasse tactique (5%)\n   Armure victorieuse (2%)", "2250 PO\n   Purpourra (vol)\n   Lingot d'or (10%)\n   Manteau d'ange (2%)", "2392 PO\n   Feuille d'yggdrasil (vol et 100%)\n   Orichalque (10%)\n   Carte de Lordragon (5%)"};
    //BossType = {"Beast", "Beast", "Slime", "Machine", "Dragon", "Zombie", "Demon", "Beast", "Bird", "Beast", "Autres", "Dragon"};
    BossType = {"Bête", "Bête", "Gluant", "Machine", "Dragon", "Zombie", "Demon", "Bête", "Oiseau", "Bête", "Autres", "Dragon"};
    //BossHp = {"1800 HP", "2800 HP", "3600 HP", "4000 HP", "4300 HP", "5200 HP", "6500 HP", "6000 HP", "5200 HP", "6600 HP", "7000 HP", "7400 HP"};
    BossHp = {"1800 PV", "2800 PV", "3600 PV", "4000 PV", "4300 PV", "5200 PV", "6500 PV", "6000 PV", "5200 PV", "6600 PV", "7000 PV", "7400 PV"};


    InitMonster();
}

void MainWindow::InitMonster()
{
    QLabel* lbl = new QLabel(MonsterUi);
    lbl->setGeometry(20,0,200,20);
    lbl->setText(Monster[BossIndex()+282]);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->show();

    QLabel* pix = new QLabel(MonsterUi);
    pix->setGeometry(20,20,200,200);
    pix->move(20,20);
    pix->setPixmap(QPixmap(":/Boss/"+QString::number(BossIndex())+".png").scaled(200,200,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    pix->show();

    uchar f = ceil((float)FloorCount()/4.0);
    int a=FloorCount()%4;
    QString txt;
    for (int i=0; i<f; i++)
    {
        if(i==(f-1) && a>1)
            txt += "Etages "+QString::number(1+i*4)+" - "+QString::number(FloorCount())+" : ";
        else if(i==(f-1) && a==1)
            txt += "Etage "+QString::number(1+i*4)+" : ";
        else
            txt += "Etages "+QString::number(1+i*4)+" - "+QString::number(4+i*4)+" : ";
        for (int j=0; j<7; j++)
        {
            uchar z =TableMonster[MapTypeIndex()][MonsterRank()-1+i][j];
            if (z!=0)
                txt+= Monster[z-1]+", ";
        }
        txt=txt.left(txt.size()-2)+"\n";
        //txt+="\n";
    }
    QLabel* lbl2 = new QLabel(MonsterUi);
    lbl2->move(20,230);
    lbl2->setText(txt);
    lbl2->show();

    QLabel* lbl3 = new QLabel(MonsterUi);
    lbl3->move(230,0);
    lbl3->setText(BossType[BossIndex()]+", "+BossHp[BossIndex()]);
    lbl3->show();

    QLabel* lbl4 = new QLabel(MonsterUi);
    lbl4->move(230,20);
    lbl4->setText("Drop :\n   "+BossDrop[BossIndex()]);
    lbl4->show();

    QLabel* lbl5 = new QLabel(MonsterUi);
    lbl5->move(230,110);
    lbl5->setText("Faiblesse :\n   "+BossWeak[BossIndex()]);
    lbl5->show();

    QPushButton* pb = new QPushButton(MonsterUi);
    pb->setGeometry(670,320,70,22);
    pb->setText("Quitter");
    connect(pb, SIGNAL(clicked()), this, SLOT(MonsterClose()));
    pb->show();
}

void MainWindow::MonsterClose()
{
    MonsterUi->close();
}

//###############################################################################################################################
//###############################                     Function Monster Info                      ################################
//###############################################################################################################################

void MainWindow::InitInfoMapUi()
{
    InfoMapUi = new QWidget(this,Qt::Popup|Qt::Dialog);
    InfoMapUi->setWindowModality(Qt::ApplicationModal);
    InfoMapUi->setWindowTitle("Information Map");
    InfoMapUi->setFixedSize(800,350);
    InfoMapUi->move(100,100);
    InfoMapUi->show();
    InitInfoMap();
}

void MainWindow::InitInfoMap()
{
    QLabel* lbl = new QLabel(InfoMapUi);
    lbl->setGeometry(20,20,768,200);
    lbl->setPixmap(QPixmap(":/Pos/"+LocToPos[Loc-1]+".jpg"));
    lbl->show();

    QLabel* lbl2 = new QLabel(InfoMapUi);
    lbl2->setGeometry(20,220,150,22);
    lbl2->setText("Numéro de la carte : "+QString::number(Loc,16).toUpper());
    lbl2->show();

    QPushButton* pb = new QPushButton(InfoMapUi);
    pb->setGeometry(720,320,70,22);
    pb->setText("Quitter");
    connect(pb, SIGNAL(clicked()), this, SLOT(InfoMapClose()));
    pb->show();
}

void MainWindow::InfoMapClose()
{
    InfoMapUi->close();
}
