#include "LevelLoader.h"

#include "Chamber.h"
#include "ClueTrigger.h"
#include "Coin.h"
#include "Enemy.h"
#include "HiddenWall.h"
#include "Level.h"
#include "TreasureRoom.h"
#include "TriggerWall.h"
#include "Wall.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <algorithm>
#include <functional>

namespace {

bool isValidGrid(const QJsonArray& rows, int expectedWidth, int expectedHeight)
{
    if (rows.size() != expectedHeight) return false;
    for (const QJsonValue& rowVal : rows) {
        if (rowVal.toArray().size() != expectedWidth) return false;
    }
    return true;
}

void carveCell(Level* level, int x, int y)
{
    if (!level->isInBounds(x, y)) return;
    level->setTileAt(x, y, static_cast<int>(CellType::Corridor));
    level->setCollisionAt(x, y, false);
}

void carveH(Level* level, int y, int x1, int x2)
{
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; ++x) carveCell(level, x, y);
}

void carveV(Level* level, int x, int y1, int y2)
{
    if (y1 > y2) std::swap(y1, y2);
    for (int y = y1; y <= y2; ++y) carveCell(level, x, y);
}

// Carves an L-shaped corridor between chamber centers.
void carveCorridor(Level* level, QPoint a, QPoint b, QRandomGenerator& rng)
{
    if (rng.bounded(2) == 0) {
        carveH(level, a.y(), a.x(), b.x());
        carveV(level, b.x(), a.y(), b.y());
    } else {
        carveV(level, a.x(), a.y(), b.y());
        carveH(level, b.y(), a.x(), b.x());
    }
}

// Carves only chamber interiors. Borders remain blocking wall tiles in the
// collision grid, which is the authoritative blocker in chamber-format levels.
void carveChamber(Level* level, const Chamber& ch)
{
    for (int y = ch.y; y < ch.y + ch.height; ++y) {
        for (int x = ch.x; x < ch.x + ch.width; ++x) {
            if (level->isInBounds(x, y)) {
                level->setTileAt(x, y, static_cast<int>(CellType::Chamber));
                level->setCollisionAt(x, y, false);
            }
        }
    }
}


struct BSPNode {
    int x, y, w, h;
    int childL = -1, childR = -1;
    bool isLeaf() const { return childL < 0 && childR < 0; }
};

QVector<BSPNode> buildBSP(int x, int y, int w, int h,
                          int depth, QRandomGenerator& rng,
                          QVector<BSPNode>& nodes)
{
    BSPNode node;
    node.x = x; node.y = y; node.w = w; node.h = h;
    const int idx = nodes.size();
    nodes.push_back(node);

    const int MIN_SIZE = 10;
    if (depth == 0 || w < MIN_SIZE * 2 || h < MIN_SIZE * 2) {
        return nodes;
    }

    // Bias split direction to reduce long, thin partitions.
    bool splitH = (h > w) ? true : (w > h ? false : rng.bounded(2) == 0);

    if (splitH) {
        const int split = MIN_SIZE + rng.bounded(h - MIN_SIZE * 2 + 1);
        nodes[idx].childL = nodes.size();
        buildBSP(x, y, w, split, depth - 1, rng, nodes);
        nodes[idx].childR = nodes.size();
        buildBSP(x, y + split, w, h - split, depth - 1, rng, nodes);
    } else {
        const int split = MIN_SIZE + rng.bounded(w - MIN_SIZE * 2 + 1);
        nodes[idx].childL = nodes.size();
        buildBSP(x, y, split, h, depth - 1, rng, nodes);
        nodes[idx].childR = nodes.size();
        buildBSP(x + split, y, w - split, h, depth - 1, rng, nodes);
    }
    return nodes;
}

QVector<Chamber> extractChambers(const QVector<BSPNode>& nodes, QRandomGenerator& rng)
{
    QVector<Chamber> chambers;
    int id = 0;
    const int MARGIN = 1;
    const int MIN_ROOM = 5;

    for (const BSPNode& node : nodes) {
        if (!node.isLeaf()) continue;
        if (node.w < MIN_ROOM + MARGIN * 2 + 2 || node.h < MIN_ROOM + MARGIN * 2 + 2)
            continue;

        const int maxW = node.w - MARGIN * 2 - 2;
        const int maxH = node.h - MARGIN * 2 - 2;
        const int rw   = MIN_ROOM + rng.bounded(qMax(1, maxW - MIN_ROOM + 1));
        const int rh   = MIN_ROOM + rng.bounded(qMax(1, maxH - MIN_ROOM + 1));
        const int rx   = node.x + MARGIN + 1 + rng.bounded(qMax(1, node.w - rw - MARGIN * 2 - 1));
        const int ry   = node.y + MARGIN + 1 + rng.bounded(qMax(1, node.h - rh - MARGIN * 2 - 1));

        Chamber ch;
        ch.id = id++;
        ch.x  = rx;
        ch.y  = ry;
        ch.width  = rw;
        ch.height = rh;
        chambers.push_back(ch);
    }
    return chambers;
}

// Connect chambers with an MST, then add extra edges for loops.
QVector<QPair<int,int>> connectChambers(const QVector<Chamber>& chambers, QRandomGenerator& rng)
{
    QVector<QPair<int,int>> edges;
    if (chambers.size() < 2) return edges;

    QVector<bool> inTree(chambers.size(), false);
    inTree[0] = true;
    int connected = 1;

    while (connected < chambers.size()) {
        int bestA = -1, bestB = -1;
        double bestDist = 1e18;

        for (int a = 0; a < chambers.size(); ++a) {
            if (!inTree[a]) continue;
            for (int b = 0; b < chambers.size(); ++b) {
                if (inTree[b]) continue;
                const QPoint da = chambers[a].centre();
                const QPoint db = chambers[b].centre();
                const int dx = da.x() - db.x();
                const int dy = da.y() - db.y();
                const double d = dx * dx + dy * dy;
                if (d < bestDist) {
                    bestDist = d;
                    bestA = a;
                    bestB = b;
                }
            }
        }

        if (bestA < 0) break;
        edges.push_back({bestA, bestB});
        inTree[bestB] = true;
        ++connected;
    }

    const int extras = qMax(0, static_cast<int>(chambers.size() * 0.3));
    for (int i = 0; i < extras; ++i) {
        const int a = rng.bounded(chambers.size());
        int b = rng.bounded(chambers.size());
        while (b == a) b = rng.bounded(chambers.size());
        edges.push_back({a, b});
    }

    return edges;
}

// Places up to one coin per chamber and always skips the spawn chamber.
void distributeCoins(QVector<Chamber>& chambers, int spawnIdx,
                     int coinCount, QRandomGenerator& rng)
{
    QVector<int> eligible;
    for (int i = 0; i < chambers.size(); ++i) {
        if (i != spawnIdx) eligible.push_back(i);
    }
    for (int i = eligible.size() - 1; i > 0; --i) {
        int j = rng.bounded(i + 1);
        std::swap(eligible[i], eligible[j]);
    }

    const int actual = qMin(coinCount, eligible.size());
    for (int k = 0; k < actual; ++k) {
        Chamber& ch = chambers[eligible[k]];
        const int cx = ch.x + 1 + rng.bounded(qMax(1, ch.width  - 2));
        const int cy = ch.y + 1 + rng.bounded(qMax(1, ch.height - 2));
        ch.coinPositions.push_back(QPoint(cx, cy));
    }
}

QJsonArray buildDefaultClues()
{
    QJsonArray clues;

    QJsonObject c1;
    c1["condition"]     = "coins";
    c1["coinsRequired"] = 2;
    c1["text"]          = "You sense the treasure drawing near... two coins collected!";
    clues.append(c1);

    QJsonObject c2;
    c2["condition"]     = "coins";
    c2["coinsRequired"] = 3;
    c2["text"]          = "The treasure room has unlocked! Find it before time runs out!";
    clues.append(c2);

    return clues;
}

Level* buildFromChamberJson(const QJsonObject& root, QJsonArray* outClues)
{
    Level* level = new Level();
    level->setName(root.value("name").toString("Chamber Level"));
    level->setSize(root.value("width").toInt(40), root.value("height").toInt(28));
    level->setTimeLimit(root.value("timeLimit").toInt(180));

    QRandomGenerator rng(static_cast<quint32>(root.value("seed").toInt(42)));

    const QJsonArray chambersJson = root.value("chambers").toArray();
    QVector<Chamber> chambers;

    for (const QJsonValue& cv : chambersJson) {
        const QJsonObject co = cv.toObject();
        Chamber ch;
        ch.id     = co.value("id").toInt();
        ch.x      = co.value("x").toInt();
        ch.y      = co.value("y").toInt();
        ch.width  = co.value("w").toInt();
        ch.height = co.value("h").toInt();
        if (co.value("hasCoin").toBool(false)) {
            const int cx = ch.x + 1 + rng.bounded(qMax(1, ch.width  - 2));
            const int cy = ch.y + 1 + rng.bounded(qMax(1, ch.height - 2));
            ch.coinPositions.push_back(QPoint(cx, cy));
        }
        if (co.value("hasTreasureRoom").toBool(false)) {
            ch.hasTreasureRoom = true;
            ch.treasurePos = QPoint(ch.x + ch.width / 2, ch.y + ch.height / 2);
        }
        if (co.value("hasEnemy").toBool(false)) {
            ch.hasEnemy = true;
        }
        chambers.push_back(ch);
    }

    const int spawnChamberIdx = root.value("spawnChamber").toInt(0);
    const QPoint spawnPos = chambers.isEmpty()
        ? QPoint(1, 1)
        : chambers[qBound(0, spawnChamberIdx, chambers.size()-1)].centre();
    level->setSpawn(spawnPos.x(), spawnPos.y());

    for (int y = 0; y < level->getHeight(); ++y)
        for (int x = 0; x < level->getWidth(); ++x) {
            level->setTileAt(x, y, static_cast<int>(CellType::Wall));
            level->setCollisionAt(x, y, true);
        }

    for (const Chamber& ch : chambers) {
        carveChamber(level, ch);
        level->addChamber(ch);
    }

    const QJsonArray corridors = root.value("corridors").toArray();
    for (const QJsonValue& cv : corridors) {
        const QJsonObject co = cv.toObject();
        const int fromIdx = co.value("from").toInt();
        const int toIdx   = co.value("to").toInt();
        if (fromIdx < chambers.size() && toIdx < chambers.size()) {
            carveCorridor(level, chambers[fromIdx].centre(),
                          chambers[toIdx].centre(), rng);
        }
    }

    for (int i = 0; i < chambers.size(); ++i) {
        const Chamber& ch = chambers[i];
        for (const QPoint& cp : ch.coinPositions) {
            level->addCoin(new Coin(cp.x(), cp.y(), 100));
        }
        if (ch.hasTreasureRoom) {
            level->setTreasureRoom(new TreasureRoom(ch.treasurePos.x(), ch.treasurePos.y()));
        }
        if (i != spawnChamberIdx && ch.hasEnemy) {
            const int ex = ch.x + 1 + rng.bounded(qMax(1, ch.width  - 2));
            const int ey = ch.y + 1 + rng.bounded(qMax(1, ch.height - 2));
            level->addEnemy(new Enemy(ex, ey));
        }
    }

    const QJsonArray hiddenWalls = root.value("hiddenWalls").toArray();
    for (const QJsonValue& value : hiddenWalls) {
        const QJsonArray xy = value.toArray();
        if (xy.size() < 2) continue;
        const int x = xy.at(0).toInt();
        const int y = xy.at(1).toInt();
        if (level->isInBounds(x, y)) {
            level->setTileAt(x, y, static_cast<int>(CellType::HiddenWall));
            level->setCollisionAt(x, y, true);
            level->addHiddenWall(new HiddenWall(x, y));
        }
    }

    // Trigger walls open target walls defined in "opensWalls".
    const QJsonArray triggerWalls = root.value("triggerWalls").toArray();
    for (const QJsonValue& value : triggerWalls) {
        const QJsonObject tw = value.toObject();
        const int x = tw.value("x").toInt();
        const int y = tw.value("y").toInt();

        QVector<QPoint> opens;
        const QJsonArray opensWalls = tw.value("opensWalls").toArray();
        for (const QJsonValue& openValue : opensWalls) {
            const QJsonArray xy = openValue.toArray();
            if (xy.size() >= 2) {
                opens.push_back(QPoint(xy.at(0).toInt(), xy.at(1).toInt()));
            }
        }

        if (level->isInBounds(x, y) && !opens.isEmpty()) {
            level->setTileAt(x, y, static_cast<int>(CellType::Wall));
            level->setCollisionAt(x, y, true);
            level->addTriggerWall(new TriggerWall(x, y, opens));
        }
    }

    const QJsonArray clues = root.value("clues").toArray().isEmpty()
        ? buildDefaultClues()
        : root.value("clues").toArray();
    if (outClues) *outClues = clues;

    for (const QJsonValue& cv : clues) {
        const QJsonObject clue = cv.toObject();
        if (clue.value("condition").toString() != "position") continue;
        const int tx = clue.value("triggerX").toInt();
        const int ty = clue.value("triggerY").toInt();
        level->addClueTrigger(new ClueTrigger(tx, ty, clue.value("text").toString()));
    }

    return level;
}

} // namespace

bool LevelLoader::isChamberFormat(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    return doc.object().value("format").toString() == "chambers";
}

Level* LevelLoader::loadFromJson(const QString& filePath, QJsonArray* outClues)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return nullptr;

    const QJsonObject root = doc.object();

    if (root.value("format").toString() == "chambers") {
        return buildFromChamberJson(root, outClues);
    }

    Level* level = new Level();
    level->setName(root.value("name").toString("Unnamed Level"));
    level->setSize(root.value("width").toInt(0), root.value("height").toInt(0));
    level->setTimeLimit(root.value("timeLimit").toInt(0));
    level->setSpawn(root.value("spawnX").toInt(0), root.value("spawnY").toInt(0));

    const int w = level->getWidth();
    const int h = level->getHeight();

    const QJsonArray tiles = root.value("tiles").toArray();
    if (!tiles.isEmpty() && isValidGrid(tiles, w, h)) {
        for (int y = 0; y < h; ++y) {
            const QJsonArray row = tiles.at(y).toArray();
            for (int x = 0; x < w; ++x)
                level->setTileAt(x, y, row.at(x).toInt(static_cast<int>(CellType::Empty)));
        }
    }

    const QJsonArray collision = root.value("collision").toArray();
    if (!collision.isEmpty() && isValidGrid(collision, w, h)) {
        for (int y = 0; y < h; ++y) {
            const QJsonArray row = collision.at(y).toArray();
            for (int x = 0; x < w; ++x) {
                const QJsonValue v = row.at(x);
                level->setCollisionAt(x, y, v.isBool() ? v.toBool() : (v.toInt(0) != 0));
            }
        }
    }

    const int treasureX = root.value("treasureX").toInt(-1);
    const int treasureY = root.value("treasureY").toInt(-1);
    if (treasureX >= 0 && treasureY >= 0)
        level->setTreasureRoom(new TreasureRoom(treasureX, treasureY));

    const QJsonArray walls = root.value("walls").toArray();
    for (const QJsonValue& value : walls) {
        const QJsonArray xy = value.toArray();
        if (xy.size() < 2) continue;
        level->addWall(new Wall(xy.at(0).toInt(), xy.at(1).toInt()));
    }

    const QJsonArray hiddenWalls = root.value("hiddenWalls").toArray();
    for (const QJsonValue& value : hiddenWalls) {
        const QJsonArray xy = value.toArray();
        if (xy.size() < 2) continue;
        level->addHiddenWall(new HiddenWall(xy.at(0).toInt(), xy.at(1).toInt()));
    }

    const QJsonArray triggerWalls = root.value("triggerWalls").toArray();
    for (const QJsonValue& value : triggerWalls) {
        const QJsonObject tw = value.toObject();
        QVector<QPoint> opens;
        const QJsonArray opensWalls = tw.value("opensWalls").toArray();
        for (const QJsonValue& openValue : opensWalls) {
            const QJsonArray xy = openValue.toArray();
            if (xy.size() >= 2)
                opens.push_back(QPoint(xy.at(0).toInt(), xy.at(1).toInt()));
        }
        level->addTriggerWall(new TriggerWall(tw.value("x").toInt(), tw.value("y").toInt(), opens));
    }

    const QJsonArray coins = root.value("coins").toArray();
    for (const QJsonValue& value : coins) {
        const QJsonObject coin = value.toObject();
        level->addCoin(new Coin(coin.value("x").toInt(), coin.value("y").toInt(),
                                coin.value("value").toInt(100)));
    }

    const QJsonArray clues = root.value("clues").toArray();
    if (outClues) *outClues = clues;
    for (const QJsonValue& value : clues) {
        const QJsonObject clue = value.toObject();
        if (clue.value("condition").toString() != "position") continue;
        level->addClueTrigger(new ClueTrigger(
            clue.value("triggerX").toInt(), clue.value("triggerY").toInt(),
            clue.value("text").toString()));
    }

    return level;
}

Level* LevelLoader::generateProcedural(int seed, int difficulty, QJsonArray* outClues)
{
    const int mapW    = (difficulty == 0) ? 36 : (difficulty == 2) ? 60 : 48;
    const int mapH    = (difficulty == 0) ? 24 : (difficulty == 2) ? 40 : 32;
    const int bspDepth= (difficulty == 0) ?  3 : (difficulty == 2) ?  5 :  4;
    const int coins   = (difficulty == 0) ?  4 : (difficulty == 2) ?  6 :  5;
    const int timeLim = (difficulty == 0) ? 180 : (difficulty == 2) ? 120 : 150;

    QRandomGenerator rng(static_cast<quint32>(seed));

    Level* level = new Level();
    level->setName(QString("Generated Level (seed %1)").arg(seed));
    level->setSize(mapW, mapH);
    level->setTimeLimit(timeLim);

    for (int y = 0; y < mapH; ++y)
        for (int x = 0; x < mapW; ++x) {
            level->setTileAt(x, y, static_cast<int>(CellType::Wall));
            level->setCollisionAt(x, y, true);
        }

    QVector<BSPNode> nodes;
    buildBSP(1, 1, mapW - 2, mapH - 2, bspDepth, rng, nodes);
    QVector<Chamber> chambers = extractChambers(nodes, rng);

    if (chambers.isEmpty()) {
        Chamber ch;
        ch.id = 0; ch.x = 2; ch.y = 2; ch.width = mapW-4; ch.height = mapH-4;
        chambers.push_back(ch);
    }

    const QPoint spawn = chambers[0].centre();
    level->setSpawn(spawn.x(), spawn.y());

    // BSP mode relies on the collision grid rather than explicit Wall objects.
    for (Chamber& ch : chambers) {
        carveChamber(level, ch);
        level->addChamber(ch);
    }

    const QVector<QPair<int,int>> edges = connectChambers(chambers, rng);
    for (const auto& edge : edges) {
        carveCorridor(level, chambers[edge.first].centre(),
                      chambers[edge.second].centre(), rng);
    }

    distributeCoins(chambers, 0, coins, rng);

    int farthest = 0;
    double maxDist = 0.0;
    for (int i = 1; i < chambers.size(); ++i) {
        const QPoint c = chambers[i].centre();
        const double d = (c.x()-spawn.x())*(c.x()-spawn.x())
                       + (c.y()-spawn.y())*(c.y()-spawn.y());
        if (d > maxDist) { maxDist = d; farthest = i; }
    }
    chambers[farthest].hasTreasureRoom = true;
    chambers[farthest].treasurePos = chambers[farthest].centre();

    for (int i = 0; i < chambers.size(); ++i) {
        Chamber& ch = chambers[i];
        for (const QPoint& cp : ch.coinPositions)
            level->addCoin(new Coin(cp.x(), cp.y(), 100));
        if (ch.hasTreasureRoom)
            level->setTreasureRoom(new TreasureRoom(ch.treasurePos.x(), ch.treasurePos.y()));

        // Increase enemy density with difficulty.
        if (i != 0 && rng.bounded(100) < (difficulty * 25 + 25)) {
            const int ex = ch.x + 1 + rng.bounded(qMax(1, ch.width  - 2));
            const int ey = ch.y + 1 + rng.bounded(qMax(1, ch.height - 2));
            level->addEnemy(new Enemy(ex, ey));
        }
    }

    const QJsonArray clues = buildDefaultClues();
    if (outClues) *outClues = clues;

    return level;
}

QStringList LevelLoader::getAvailableLevels(const QString& levelsDir)
{
    QDir dir(levelsDir);
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
    QStringList result;
    result.reserve(files.size());
    for (const QString& file : files)
        result.push_back(dir.absoluteFilePath(file));
    return result;
}
