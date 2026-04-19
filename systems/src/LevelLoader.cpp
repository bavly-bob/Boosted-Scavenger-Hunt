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
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>
#include <QDebug>
#include <algorithm>
#include <functional>
#include <limits>

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

quint64 jsonCellKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint64>(static_cast<quint32>(y));
}

QVector<int> readIntList(const QJsonArray& values)
{
    QVector<int> out;
    out.reserve(values.size());
    for (const QJsonValue& value : values) {
        out.push_back(value.toInt(-1));
    }
    return out;
}

struct HiddenWallJsonSpec {
    int id = -1;
    QPoint pos = QPoint(-1, -1);
    int connectedNodeA = -1;
    int connectedNodeB = -1;
    QVector<int> requiredTriggerIds;
};

struct TriggerWallJsonSpec {
    int id = -1;
    QPoint pos = QPoint(-1, -1);
    QVector<int> wallIds;
};

QVector<HiddenWallJsonSpec> parseHiddenWallSpecs(const QJsonArray& hiddenWalls,
                                                 const Level* level,
                                                 QHash<quint64, int>& wallIdByCell)
{
    QVector<HiddenWallJsonSpec> specs;
    QSet<int> usedIds;
    int nextId = 0;

    auto reserveUniqueWallId = [&](int requestedId) {
        int id = requestedId;
        if (id < 0 || usedIds.contains(id)) {
            if (id >= 0 && usedIds.contains(id)) {
                qWarning() << "Duplicate hidden wall ID in JSON:" << id << "- reassigning deterministically";
            }
            while (usedIds.contains(nextId)) {
                ++nextId;
            }
            id = nextId++;
        }
        usedIds.insert(id);
        if (id >= nextId) {
            nextId = id + 1;
        }
        return id;
    };

    for (const QJsonValue& value : hiddenWalls) {
        HiddenWallJsonSpec spec;
        if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            spec.id = reserveUniqueWallId(obj.value("id").toInt(-1));
            spec.pos = QPoint(obj.value("x").toInt(-1), obj.value("y").toInt(-1));
            if (spec.pos.x() < 0 || spec.pos.y() < 0) {
                const QJsonArray pos = obj.value("pos").toArray();
                if (pos.size() >= 2) {
                    spec.pos = QPoint(pos.at(0).toInt(-1), pos.at(1).toInt(-1));
                }
            }
            spec.connectedNodeA = obj.value("connectedNodeA").toInt(-1);
            spec.connectedNodeB = obj.value("connectedNodeB").toInt(-1);
            spec.requiredTriggerIds = readIntList(obj.value("requiredTriggerIds").toArray());
        } else {
            const QJsonArray xy = value.toArray();
            if (xy.size() < 2) {
                continue;
            }
            spec.id = reserveUniqueWallId(-1);
            spec.pos = QPoint(xy.at(0).toInt(-1), xy.at(1).toInt(-1));
        }

        if (!level->isInBounds(spec.pos.x(), spec.pos.y())) {
            qWarning() << "Skipping hidden wall outside bounds at" << spec.pos;
            continue;
        }

        const quint64 key = jsonCellKey(spec.pos.x(), spec.pos.y());
        if (wallIdByCell.contains(key)) {
            qWarning() << "Duplicate hidden wall position in JSON at" << spec.pos
                       << "- keeping first wall ID" << wallIdByCell.value(key);
            continue;
        }
        wallIdByCell.insert(key, spec.id);
        specs.push_back(spec);
    }

    return specs;
}

QVector<TriggerWallJsonSpec> parseTriggerWallSpecs(const QJsonArray& triggerWalls,
                                                   const Level* level,
                                                   const QHash<quint64, int>& wallIdByCell)
{
    QVector<TriggerWallJsonSpec> specs;
    QSet<int> usedIds;
    int nextId = 0;

    auto reserveUniqueTriggerId = [&](int requestedId) {
        int id = requestedId;
        if (id < 0 || usedIds.contains(id)) {
            if (id >= 0 && usedIds.contains(id)) {
                qWarning() << "Duplicate trigger ID in JSON:" << id << "- reassigning deterministically";
            }
            while (usedIds.contains(nextId)) {
                ++nextId;
            }
            id = nextId++;
        }
        usedIds.insert(id);
        if (id >= nextId) {
            nextId = id + 1;
        }
        return id;
    };

    for (const QJsonValue& value : triggerWalls) {
        if (!value.isObject()) {
            continue;
        }

        TriggerWallJsonSpec spec;
        const QJsonObject obj = value.toObject();
        spec.id = reserveUniqueTriggerId(obj.value("id").toInt(-1));
        spec.pos = QPoint(obj.value("x").toInt(-1), obj.value("y").toInt(-1));
        if (!level->isInBounds(spec.pos.x(), spec.pos.y())) {
            qWarning() << "Skipping trigger outside bounds at" << spec.pos;
            continue;
        }

        spec.wallIds = readIntList(obj.value("controlsWallIds").toArray());
        if (spec.wallIds.isEmpty()) {
            const QJsonArray opensWalls = obj.value("opensWalls").toArray();
            for (const QJsonValue& openValue : opensWalls) {
                const QJsonArray xy = openValue.toArray();
                if (xy.size() < 2) {
                    continue;
                }
                const QPoint target(xy.at(0).toInt(-1), xy.at(1).toInt(-1));
                const int wallId = wallIdByCell.value(jsonCellKey(target.x(), target.y()), -1);
                if (wallId < 0) {
                    qWarning() << "Trigger" << spec.id
                               << "references missing wall at" << target;
                    continue;
                }
                spec.wallIds.push_back(wallId);
            }
        }

        std::sort(spec.wallIds.begin(), spec.wallIds.end());
        spec.wallIds.erase(std::unique(spec.wallIds.begin(), spec.wallIds.end()), spec.wallIds.end());
        specs.push_back(spec);
    }

    return specs;
}

void applyHiddenAndTriggerSpecs(Level* level,
                                const QVector<HiddenWallJsonSpec>& hiddenSpecs,
                                const QVector<TriggerWallJsonSpec>& triggerSpecs)
{
    QHash<int, QSet<int>> triggerIdsByWall;
    for (const TriggerWallJsonSpec& triggerSpec : triggerSpecs) {
        for (int wallId : triggerSpec.wallIds) {
            triggerIdsByWall[wallId].insert(triggerSpec.id);
        }
    }

    for (const HiddenWallJsonSpec& hiddenSpec : hiddenSpecs) {
        HiddenWall* wall = new HiddenWall(hiddenSpec.pos.x(),
                                          hiddenSpec.pos.y(),
                                          hiddenSpec.id,
                                          hiddenSpec.connectedNodeA,
                                          hiddenSpec.connectedNodeB);
        if (!hiddenSpec.requiredTriggerIds.isEmpty()) {
            wall->setRequiredTriggerIds(hiddenSpec.requiredTriggerIds);
        } else {
            wall->setRequiredTriggerIds(triggerIdsByWall.value(hiddenSpec.id).values().toVector());
        }
        level->addHiddenWall(wall);
    }

    for (const TriggerWallJsonSpec& triggerSpec : triggerSpecs) {
        level->addTriggerWall(new TriggerWall(triggerSpec.pos.x(),
                                              triggerSpec.pos.y(),
                                              triggerSpec.id,
                                              triggerSpec.wallIds));
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
    const QJsonArray triggerWalls = root.value("triggerWalls").toArray();
    QHash<quint64, int> wallIdByCell;
    const QVector<HiddenWallJsonSpec> hiddenSpecs = parseHiddenWallSpecs(hiddenWalls, level, wallIdByCell);
    const QVector<TriggerWallJsonSpec> triggerSpecs = parseTriggerWallSpecs(triggerWalls, level, wallIdByCell);
    applyHiddenAndTriggerSpecs(level, hiddenSpecs, triggerSpecs);
    level->validateTriggerWallConsistency();

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
    const QJsonArray triggerWalls = root.value("triggerWalls").toArray();
    QHash<quint64, int> wallIdByCell;
    const QVector<HiddenWallJsonSpec> hiddenSpecs = parseHiddenWallSpecs(hiddenWalls, level, wallIdByCell);
    const QVector<TriggerWallJsonSpec> triggerSpecs = parseTriggerWallSpecs(triggerWalls, level, wallIdByCell);
    applyHiddenAndTriggerSpecs(level, hiddenSpecs, triggerSpecs);
    level->validateTriggerWallConsistency();

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

namespace {

struct GraphEdge {
    int a = 0;
    int b = 0;
    bool gated = false;
    bool risky = false;
};

struct NodeAnchor {
    double nx = 0.5;
    double ny = 0.5;
};

struct DifficultyTemplate {
    QString name;
    int mapW = 48;
    int mapH = 32;
    int timeLimit = 150;
    int coinTarget = 5;
    int enemyTarget = 4;
    QVector<NodeAnchor> anchors;
    QVector<GraphEdge> edges;
    QVector<int> rewardDeadEnds;
    QVector<int> trapDeadEnds;
    QVector<int> triggerRooms;
    QVector<int> riskyNodes;
};

quint64 makePairKey(int a, int b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<quint64>(static_cast<quint32>(a)) << 32)
         | static_cast<quint64>(static_cast<quint32>(b));
}

quint64 makeCellKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint64>(static_cast<quint32>(y));
}

bool chamberContains(const Chamber& ch, const QPoint& p)
{
    return p.x() >= ch.x && p.x() < (ch.x + ch.width)
        && p.y() >= ch.y && p.y() < (ch.y + ch.height);
}

bool overlaps(const Chamber& a, const Chamber& b, int padding)
{
    const int ax2 = a.x + a.width;
    const int ay2 = a.y + a.height;
    const int bx2 = b.x + b.width;
    const int by2 = b.y + b.height;

    return !(ax2 + padding <= b.x || bx2 + padding <= a.x
          || ay2 + padding <= b.y || by2 + padding <= a.y);
}

void appendUnique(QVector<int>& out, int value)
{
    if (!out.contains(value)) out.push_back(value);
}

void appendUnique(QVector<int>& out, const QVector<int>& values)
{
    for (int v : values) appendUnique(out, v);
}

DifficultyTemplate buildTemplate(int difficultyTier)
{
    DifficultyTemplate t;
    auto addEdge = [&](int a, int b, bool gated = false, bool risky = false) {
        GraphEdge e;
        e.a = a;
        e.b = b;
        e.gated = gated;
        e.risky = risky;
        t.edges.push_back(e);
    };

    if (difficultyTier <= 0) {
        t.name = "Generated Easy Caves";
        t.mapW = 46;
        t.mapH = 32;
        t.timeLimit = 170;
        t.coinTarget = 4;
        t.enemyTarget = 2;

        t.anchors = {
            {0.48, 0.50}, // 0 spawn hub
            {0.28, 0.40}, // 1 west route
            {0.62, 0.38}, // 2 east route
            {0.18, 0.24}, // 3 reward dead-end + trigger room
            {0.44, 0.68}, // 4 lower route
            {0.78, 0.62}, // 5 treasure side
            {0.20, 0.76}, // 6 reward dead-end
            {0.74, 0.24}  // 7 enemy pen
        };

        addEdge(0, 1); addEdge(0, 2);
        addEdge(1, 4); addEdge(2, 4);
        addEdge(2, 5); addEdge(4, 5);
        addEdge(1, 3); addEdge(4, 6);

        // Optional shortcut + optional danger release.
        addEdge(1, 5, true, true);
        addEdge(2, 7, true, false);

        t.rewardDeadEnds = {3, 6};
        t.trapDeadEnds = {7};
        t.triggerRooms = {3};
        t.riskyNodes = {2, 5, 7};
    } else if (difficultyTier >= 2) {
        t.name = "Generated Hard Labyrinth";
        t.mapW = 74;
        t.mapH = 52;
        t.timeLimit = 118;
        t.coinTarget = 7;
        t.enemyTarget = 8;

        t.anchors = {
            {0.48, 0.50}, // 0 spawn hub
            {0.30, 0.36}, // 1
            {0.60, 0.34}, // 2
            {0.18, 0.28}, // 3 reward dead-end
            {0.36, 0.44}, // 4
            {0.58, 0.44}, // 5
            {0.20, 0.48}, // 6 trap dead-end
            {0.34, 0.28}, // 7
            {0.58, 0.27}, // 8
            {0.72, 0.18}, // 9 reward + trigger
            {0.36, 0.62}, // 10
            {0.60, 0.62}, // 11
            {0.74, 0.66}, // 12 trap dead-end
            {0.46, 0.76}, // 13 late west
            {0.66, 0.76}, // 14 late east
            {0.84, 0.72}, // 15 treasure
            {0.50, 0.14}  // 16 trigger room
        };

        addEdge(0, 1); addEdge(0, 2);
        addEdge(1, 4); addEdge(2, 5);
        addEdge(4, 10); addEdge(5, 11);
        addEdge(10, 13); addEdge(11, 14);
        addEdge(13, 15); addEdge(14, 15);

        addEdge(1, 2); addEdge(4, 5); addEdge(10, 11); addEdge(13, 14);
        addEdge(4, 7); addEdge(5, 8); addEdge(7, 8);
        addEdge(1, 3); addEdge(8, 9); addEdge(8, 16);

        // Controlled dead-ends and enemy release pens.
        addEdge(4, 6, true, false);     // pen release on trigger
        addEdge(11, 12, true, false);   // pen release on trigger

        // High-risk shortcuts that significantly reduce travel when opened.
        addEdge(4, 13, true, true);
        addEdge(5, 14, true, true);

        // Chokepoint cross-link for pressure at key decisions.
        addEdge(5, 10, false, true);

        t.rewardDeadEnds = {3, 9};
        t.trapDeadEnds = {6, 12};
        t.triggerRooms = {9, 16};
        t.riskyNodes = {4, 5, 7, 8, 10, 11, 13, 14, 16};
    } else {
        t.name = "Generated Medium Dungeon";
        t.mapW = 60;
        t.mapH = 42;
        t.timeLimit = 150;
        t.coinTarget = 5;
        t.enemyTarget = 4;

        t.anchors = {
            {0.48, 0.50}, // 0 spawn hub
            {0.30, 0.36}, // 1
            {0.62, 0.34}, // 2
            {0.46, 0.28}, // 3
            {0.42, 0.60}, // 4
            {0.64, 0.58}, // 5
            {0.54, 0.72}, // 6
            {0.28, 0.74}, // 7 reward dead-end
            {0.74, 0.20}, // 8 challenge + trigger room
            {0.78, 0.34}, // 9 enemy pen
            {0.86, 0.62}  // 10 treasure
        };

        addEdge(0, 1); addEdge(0, 2);
        addEdge(1, 3); addEdge(2, 3);
        addEdge(3, 4); addEdge(4, 6); addEdge(6, 5); addEdge(5, 10);

        // Risky shortcut and release gate.
        addEdge(3, 5, true, true);
        addEdge(2, 9, true, true);

        addEdge(4, 7); // reward dead-end
        addEdge(3, 8); // challenge dead-end

        t.rewardDeadEnds = {7};
        t.trapDeadEnds = {9};
        t.triggerRooms = {8};
        t.riskyNodes = {3, 5, 8, 9};
    }

    return t;
}

QVector<Chamber> buildTemplateChambers(const DifficultyTemplate& tpl, QRandomGenerator& rng)
{
    QVector<Chamber> out;
    out.reserve(tpl.anchors.size());

    const int minSize = (tpl.mapW >= 70) ? 6 : 7;
    const int maxSize = (tpl.mapW >= 70) ? 9 : 10;

    for (int i = 0; i < tpl.anchors.size(); ++i) {
        const NodeAnchor anchor = tpl.anchors.at(i);
        const int cx = qBound(4, static_cast<int>(anchor.nx * (tpl.mapW - 1)), tpl.mapW - 5);
        const int cy = qBound(4, static_cast<int>(anchor.ny * (tpl.mapH - 1)), tpl.mapH - 5);

        Chamber placed;
        bool ok = false;
        for (int attempt = 0; attempt < 64 && !ok; ++attempt) {
            const int w = minSize + rng.bounded(maxSize - minSize + 1);
            const int h = minSize + rng.bounded(maxSize - minSize + 1);
            const int jitter = (tpl.mapW >= 70) ? 3 : 2;
            const int jx = rng.bounded(jitter * 2 + 1) - jitter;
            const int jy = rng.bounded(jitter * 2 + 1) - jitter;

            Chamber ch;
            ch.id = i;
            ch.width = w;
            ch.height = h;
            ch.x = qBound(1, cx - w / 2 + jx, tpl.mapW - w - 2);
            ch.y = qBound(1, cy - h / 2 + jy, tpl.mapH - h - 2);

            bool collides = false;
            for (const Chamber& other : out) {
                if (overlaps(ch, other, 1)) {
                    collides = true;
                    break;
                }
            }
            if (!collides) {
                placed = ch;
                ok = true;
            }
        }

        if (!ok) {
            placed.id = i;
            placed.width = minSize;
            placed.height = minSize;
            placed.x = qBound(1, cx - placed.width / 2, tpl.mapW - placed.width - 2);
            placed.y = qBound(1, cy - placed.height / 2, tpl.mapH - placed.height - 2);
        }

        out.push_back(placed);
    }

    return out;
}

QVector<QPoint> carveCorridorWithPath(Level* level, const QPoint& a, const QPoint& b, bool horizontalFirst)
{
    QVector<QPoint> path;
    QSet<quint64> seen;

    auto carveAndRecord = [&](int x, int y) {
        if (!level->isInBounds(x, y)) return;
        carveCell(level, x, y);
        const quint64 key = makeCellKey(x, y);
        if (!seen.contains(key)) {
            seen.insert(key);
            path.push_back(QPoint(x, y));
        }
    };

    auto walkH = [&](int y, int x1, int x2) {
        const int step = (x2 >= x1) ? 1 : -1;
        for (int x = x1;; x += step) {
            carveAndRecord(x, y);
            if (x == x2) break;
        }
    };

    auto walkV = [&](int x, int y1, int y2) {
        const int step = (y2 >= y1) ? 1 : -1;
        for (int y = y1;; y += step) {
            carveAndRecord(x, y);
            if (y == y2) break;
        }
    };

    if (horizontalFirst) {
        walkH(a.y(), a.x(), b.x());
        walkV(b.x(), a.y(), b.y());
    } else {
        walkV(a.x(), a.y(), b.y());
        walkH(b.y(), a.x(), b.x());
    }

    return path;
}

QVector<QVector<int>> buildAdjacency(int nodeCount, const QVector<GraphEdge>& edges)
{
    QVector<QVector<int>> adj(nodeCount);
    for (const GraphEdge& e : edges) {
        if (e.a < 0 || e.b < 0 || e.a >= nodeCount || e.b >= nodeCount) continue;
        if (!adj[e.a].contains(e.b)) adj[e.a].push_back(e.b);
        if (!adj[e.b].contains(e.a)) adj[e.b].push_back(e.a);
    }
    return adj;
}

QVector<int> bfsDepths(const QVector<QVector<int>>& adj, int start)
{
    QVector<int> depth(adj.size(), -1);
    if (start < 0 || start >= adj.size()) return depth;

    QQueue<int> q;
    depth[start] = 0;
    q.enqueue(start);

    while (!q.isEmpty()) {
        const int cur = q.dequeue();
        for (int nxt : adj[cur]) {
            if (depth[nxt] >= 0) continue;
            depth[nxt] = depth[cur] + 1;
            q.enqueue(nxt);
        }
    }

    return depth;
}

QVector<int> zonesFromDepths(const QVector<int>& depths)
{
    int maxDepth = 0;
    for (int d : depths) maxDepth = qMax(maxDepth, d);

    const int earlyCut = qMax(1, maxDepth / 3);
    const int midCut = qMax(2, (maxDepth * 2) / 3);

    QVector<int> zones(depths.size(), 0);
    for (int i = 0; i < depths.size(); ++i) {
        if (depths[i] <= earlyCut) {
            zones[i] = 0; // early
        } else if (depths[i] <= midCut) {
            zones[i] = 1; // mid
        } else {
            zones[i] = 2; // late
        }
    }
    return zones;
}

int chooseSpawnNode(const QVector<Chamber>& chambers, const QVector<QVector<int>>& adj, int mapW, int mapH)
{
    const QPoint center(mapW / 2, mapH / 2);
    int best = 0;
    double bestScore = std::numeric_limits<double>::max();

    for (int i = 0; i < chambers.size(); ++i) {
        if (adj[i].size() < 2) continue;
        const QPoint c = chambers[i].centre();
        const int dx = c.x() - center.x();
        const int dy = c.y() - center.y();
        const double dist2 = dx * dx + dy * dy;
        const double score = dist2 - static_cast<double>(adj[i].size()) * 6.0;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

int chooseTreasureNode(const QVector<int>& depths, const QVector<int>& zones, const QSet<int>& riskyNodes, int spawnNode)
{
    int best = spawnNode;
    int bestScore = std::numeric_limits<int>::min();
    for (int i = 0; i < depths.size(); ++i) {
        if (i == spawnNode || depths[i] < 0) continue;
        int score = depths[i] * 100;
        if (zones[i] == 2) score += 40;
        if (riskyNodes.contains(i)) score += 15;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

struct EdgeDoorways {
    QPoint nearA = QPoint(-1, -1);
    QPoint nearB = QPoint(-1, -1);
};

struct GatePlacement {
    int edgeIndex = -1;
    int nodeA = -1;
    int nodeB = -1;
    int gatedChamber = -1;
    QPoint gateCell = QPoint(-1, -1);
};

QPoint findExitCellFromChamber(const QVector<QPoint>& path, const Chamber& chamber, bool fromStart)
{
    if (path.size() < 2) return QPoint(-1, -1);

    if (fromStart) {
        for (int i = 1; i < path.size(); ++i) {
            const QPoint prev = path.at(i - 1);
            const QPoint cur = path.at(i);
            if (chamberContains(chamber, prev) && !chamberContains(chamber, cur)) {
                return cur;
            }
        }
    } else {
        for (int i = path.size() - 2; i >= 0; --i) {
            const QPoint prev = path.at(i + 1);
            const QPoint cur = path.at(i);
            if (chamberContains(chamber, prev) && !chamberContains(chamber, cur)) {
                return cur;
            }
        }
    }

    return QPoint(-1, -1);
}

EdgeDoorways resolveEdgeDoorways(const QVector<QPoint>& path, const Chamber& a, const Chamber& b)
{
    EdgeDoorways doors;
    doors.nearA = findExitCellFromChamber(path, a, true);
    doors.nearB = findExitCellFromChamber(path, b, false);
    return doors;
}

bool isValidGateTile(const Level* level, const QPoint& p)
{
    if (!level || !level->isInBounds(p.x(), p.y())) return false;
    return static_cast<CellType>(level->tileAt(p.x(), p.y())) == CellType::Corridor;
}

void appendUniquePoint(QVector<QPoint>& out, const QPoint& value)
{
    if (!out.contains(value)) {
        out.push_back(value);
    }
}

QPoint pickPointInChamber(const Chamber& ch, QRandomGenerator& rng, const QSet<quint64>& occupied, int margin = 1)
{
    const int xMin = ch.x + margin;
    const int yMin = ch.y + margin;
    const int xMax = ch.x + ch.width - 1 - margin;
    const int yMax = ch.y + ch.height - 1 - margin;
    if (xMin > xMax || yMin > yMax) return QPoint(-1, -1);

    for (int attempt = 0; attempt < 48; ++attempt) {
        const int x = xMin + rng.bounded(xMax - xMin + 1);
        const int y = yMin + rng.bounded(yMax - yMin + 1);
        if (!occupied.contains(makeCellKey(x, y))) {
            return QPoint(x, y);
        }
    }

    for (int y = yMin; y <= yMax; ++y) {
        for (int x = xMin; x <= xMax; ++x) {
            if (!occupied.contains(makeCellKey(x, y))) {
                return QPoint(x, y);
            }
        }
    }

    return QPoint(-1, -1);
}

QJsonObject makePositionClue(const QPoint& p, const QString& text)
{
    QJsonObject clue;
    clue["condition"] = "position";
    clue["triggerX"] = p.x();
    clue["triggerY"] = p.y();
    clue["text"] = text;
    return clue;
}

void appendCoinClues(QJsonArray& clues, int difficultyTier)
{
    QJsonObject c1;
    c1["condition"] = "coins";
    c1["coinsRequired"] = 2;
    c1["text"] = "Two relics found. Hidden mechanisms may alter your routes.";
    clues.append(c1);

    QJsonObject c2;
    c2["condition"] = "coins";
    c2["coinsRequired"] = 3;
    c2["text"] = "Vault seal weakened. The treasure room can now be entered.";
    clues.append(c2);

    if (difficultyTier >= 2) {
        QJsonObject c3;
        c3["condition"] = "coins";
        c3["coinsRequired"] = 5;
        c3["text"] = "You mapped the danger zones. Shortcuts now outweigh safe detours.";
        clues.append(c3);
    }
}

} // namespace

Level* LevelLoader::generateProcedural(int seed, int difficulty, QJsonArray* outClues)
{
    ProceduralGenerationContext defaultContext;
    return generateProcedural(seed, difficulty, defaultContext, outClues, nullptr);
}

Level* LevelLoader::generateProcedural(int seed,
                                       int difficulty,
                                       const ProceduralGenerationContext& context,
                                       QJsonArray* outClues,
                                       bool* outLootRoomSpawned)
{
    if (outLootRoomSpawned) {
        *outLootRoomSpawned = false;
    }

    const int difficultyTier = (difficulty <= 0) ? 0 : (difficulty >= 2) ? 2 : 1;
    const DifficultyTemplate tpl = buildTemplate(difficultyTier);
    QRandomGenerator rng(static_cast<quint32>(seed));

    const double hiddenWallFrequency = qBound(0.0, context.rules.hiddenWalls.frequency, 1.0);
    const int minGeneralTriggers = qMax(1, context.rules.generalTriggers.min);
    const int maxGeneralTriggers = qMax(minGeneralTriggers, context.rules.generalTriggers.max);
    const int lootTriggerCountRequired = qMax(2, context.rules.lootRoom.triggerCountRequired);
    const double lootSpawnFrequency = qBound(0.0, context.rules.lootRoom.spawnFrequency, 1.0);

    Level* level = new Level();
    level->setName(QString("%1 (seed %2)").arg(tpl.name).arg(seed));
    level->setSize(tpl.mapW, tpl.mapH);
    level->setTimeLimit(tpl.timeLimit);

    for (int y = 0; y < tpl.mapH; ++y) {
        for (int x = 0; x < tpl.mapW; ++x) {
            level->setTileAt(x, y, static_cast<int>(CellType::Wall));
            level->setCollisionAt(x, y, true);
        }
    }

    QVector<Chamber> chambers = buildTemplateChambers(tpl, rng);
    if (chambers.isEmpty()) {
        delete level;
        if (outClues) *outClues = QJsonArray();
        return nullptr;
    }

    for (const Chamber& ch : chambers) {
        carveChamber(level, ch);
        level->addChamber(ch);
    }

    struct EdgePlan {
        GraphEdge edge;
        int templateIndex = -1;
        bool active = true;
        bool generalGate = false;
        bool lootGate = false;
        int gatedRoom = -1;
    };
    QVector<EdgePlan> plans;
    plans.reserve(tpl.edges.size());
    for (int i = 0; i < tpl.edges.size(); ++i) {
        EdgePlan p;
        p.edge = tpl.edges.at(i);
        p.templateIndex = i;
        plans.push_back(p);
    }

    auto buildAdjFromPlans = [&](bool onlyActive) {
        QVector<GraphEdge> edges;
        edges.reserve(plans.size());
        for (const EdgePlan& p : plans) {
            if (onlyActive && !p.active) {
                continue;
            }
            edges.push_back(p.edge);
        }
        return buildAdjacency(chambers.size(), edges);
    };

    const QVector<QVector<int>> fullAdj = buildAdjFromPlans(false);
    const int provisionalSpawn = chooseSpawnNode(chambers, fullAdj, tpl.mapW, tpl.mapH);
    const QVector<int> fullDepths = bfsDepths(fullAdj, provisionalSpawn);

    QVector<int> candidateGeneralGateEdges;
    for (int i = 0; i < plans.size(); ++i) {
        if (plans[i].edge.gated) {
            candidateGeneralGateEdges.push_back(i);
        }
    }
    std::sort(candidateGeneralGateEdges.begin(), candidateGeneralGateEdges.end(), [&](int lhs, int rhs) {
        const EdgePlan& a = plans[lhs];
        const EdgePlan& b = plans[rhs];
        const int aDepth = qMax(fullDepths.value(a.edge.a, -1), fullDepths.value(a.edge.b, -1));
        const int bDepth = qMax(fullDepths.value(b.edge.a, -1), fullDepths.value(b.edge.b, -1));
        if (aDepth != bDepth) {
            return aDepth > bDepth;
        }
        return lhs < rhs;
    });

    const int desiredGeneralGates = qBound(0,
                                           qRound(hiddenWallFrequency * candidateGeneralGateEdges.size()),
                                           candidateGeneralGateEdges.size());
    QSet<int> reservedGatedRooms;
    int selectedGeneralGates = 0;
    for (int planIndex : candidateGeneralGateEdges) {
        if (selectedGeneralGates >= desiredGeneralGates) {
            break;
        }
        if (!plans[planIndex].active) {
            continue;
        }

        const QVector<QVector<int>> currentAdj = buildAdjFromPlans(true);
        QVector<int> endpoints = {plans[planIndex].edge.a, plans[planIndex].edge.b};
        std::sort(endpoints.begin(), endpoints.end(), [&](int lhs, int rhs) {
            if (fullDepths.value(lhs, -1) != fullDepths.value(rhs, -1)) {
                return fullDepths.value(lhs, -1) > fullDepths.value(rhs, -1);
            }
            return lhs > rhs;
        });

        int chosenRoom = -1;
        for (int node : endpoints) {
            if (node == provisionalSpawn) {
                continue;
            }
            if (reservedGatedRooms.contains(node)) {
                continue;
            }
            if (currentAdj.value(node).size() <= 1) {
                continue;
            }
            chosenRoom = node;
            break;
        }
        if (chosenRoom < 0) {
            continue;
        }

        plans[planIndex].generalGate = true;
        plans[planIndex].gatedRoom = chosenRoom;
        reservedGatedRooms.insert(chosenRoom);
        ++selectedGeneralGates;

        for (int i = 0; i < plans.size(); ++i) {
            if (i == planIndex || !plans[i].active) {
                continue;
            }
            const GraphEdge& e = plans[i].edge;
            if (e.a == chosenRoom || e.b == chosenRoom) {
                plans[i].active = false;
            }
        }
    }

    int lootRoomNode = -1;
    int lootGatePlanIndex = -1;
    bool lootRoomSpawned = false;
    if (context.rules.lootRoom.maxPerRun > 0
        && context.lootRoomsSpawnedThisRun < context.rules.lootRoom.maxPerRun
        && lootSpawnFrequency > 0.0) {
        quint32 rollSeed = static_cast<quint32>(seed);
        rollSeed ^= context.runSeedBase * 0x9e3779b9u;
        rollSeed ^= static_cast<quint32>(context.runIndex + 1) * 0x85ebca6bu;
        rollSeed ^= static_cast<quint32>(context.levelIndex + 1) * 0xc2b2ae35u;
        const double roll = static_cast<double>(rollSeed % 10000u) / 10000.0;
        if (roll < lootSpawnFrequency) {
            QVector<QVector<int>> adjBeforeLoot = buildAdjFromPlans(true);
            const int spawnForLootPass = chooseSpawnNode(chambers, adjBeforeLoot, tpl.mapW, tpl.mapH);
            const QVector<int> depthsForLootPass = bfsDepths(adjBeforeLoot, spawnForLootPass);
            const QVector<int> zonesForLootPass = zonesFromDepths(depthsForLootPass);

            int bestLootScore = std::numeric_limits<int>::min();
            for (int node = 0; node < chambers.size(); ++node) {
                if (node == spawnForLootPass) {
                    continue;
                }
                if (adjBeforeLoot.value(node).size() < 2) {
                    continue;
                }
                if (depthsForLootPass.value(node, -1) < 2) {
                    continue;
                }
                if (zonesForLootPass.value(node, 0) != 2) {
                    continue;
                }
                int score = depthsForLootPass[node] * 100 + adjBeforeLoot[node].size();
                if (score > bestLootScore) {
                    bestLootScore = score;
                    lootRoomNode = node;
                }
            }

            if (lootRoomNode >= 0) {
                int keptNeighbour = -1;
                int bestNeighbourDepth = std::numeric_limits<int>::max();
                for (int neighbour : adjBeforeLoot.value(lootRoomNode)) {
                    const int neighbourDepth = depthsForLootPass.value(neighbour, std::numeric_limits<int>::max());
                    if (neighbourDepth < bestNeighbourDepth) {
                        bestNeighbourDepth = neighbourDepth;
                        keptNeighbour = neighbour;
                    }
                }

                if (keptNeighbour >= 0) {
                    for (int i = 0; i < plans.size(); ++i) {
                        if (!plans[i].active) {
                            continue;
                        }
                        const GraphEdge& e = plans[i].edge;
                        const bool touchesLoot = (e.a == lootRoomNode || e.b == lootRoomNode);
                        const bool isKeptEdge =
                            (e.a == lootRoomNode && e.b == keptNeighbour)
                            || (e.b == lootRoomNode && e.a == keptNeighbour);
                        if (touchesLoot && !isKeptEdge) {
                            plans[i].active = false;
                        }
                        if (isKeptEdge) {
                            lootGatePlanIndex = i;
                        }
                    }
                }

                if (lootGatePlanIndex >= 0 && plans[lootGatePlanIndex].active) {
                    plans[lootGatePlanIndex].lootGate = true;
                    plans[lootGatePlanIndex].gatedRoom = lootRoomNode;
                    lootRoomSpawned = true;
                } else {
                    lootRoomNode = -1;
                }
            }
        }
    }

    QHash<int, EdgeDoorways> edgeDoorwaysByPlan;
    QVector<GraphEdge> activeEdges;
    for (int i = 0; i < plans.size(); ++i) {
        if (!plans[i].active) {
            continue;
        }
        const GraphEdge& e = plans[i].edge;
        if (e.a < 0 || e.b < 0 || e.a >= chambers.size() || e.b >= chambers.size()) {
            continue;
        }
        const bool horizontalFirst = (rng.bounded(100) + e.a * 11 + e.b * 17 + i * 5) % 2 == 0;
        const QVector<QPoint> path = carveCorridorWithPath(level,
                                                           chambers[e.a].centre(),
                                                           chambers[e.b].centre(),
                                                           horizontalFirst);
        edgeDoorwaysByPlan.insert(i, resolveEdgeDoorways(path, chambers[e.a], chambers[e.b]));
        activeEdges.push_back(e);
    }

    const QVector<QVector<int>> adj = buildAdjacency(chambers.size(), activeEdges);
    const int spawnNode = chooseSpawnNode(chambers, adj, tpl.mapW, tpl.mapH);
    const QPoint spawn = chambers[spawnNode].centre();
    level->setSpawn(spawn.x(), spawn.y());

    const QVector<int> depths = bfsDepths(adj, spawnNode);
    const QVector<int> zones = zonesFromDepths(depths);
    QSet<int> riskySet;
    for (int n : tpl.riskyNodes) {
        riskySet.insert(n);
    }

    int treasureNode = spawnNode;
    int bestTreasureScore = std::numeric_limits<int>::min();
    for (int i = 0; i < depths.size(); ++i) {
        if (i == spawnNode || i == lootRoomNode || depths[i] < 0) {
            continue;
        }
        int score = depths[i] * 100;
        if (zones.value(i, 0) == 2) {
            score += 40;
        }
        if (riskySet.contains(i)) {
            score += 15;
        }
        if (score > bestTreasureScore) {
            bestTreasureScore = score;
            treasureNode = i;
        }
    }

    const QPoint treasurePos = chambers[treasureNode].centre();
    level->setTreasureRoom(new TreasureRoom(treasurePos.x(), treasurePos.y()));

    QSet<quint64> occupied;
    occupied.insert(makeCellKey(spawn.x(), spawn.y()));
    occupied.insert(makeCellKey(treasurePos.x(), treasurePos.y()));

    struct PlacedGate {
        int wallId = -1;
        int planIndex = -1;
        int gatedRoom = -1;
        QPoint gateCell = QPoint(-1, -1);
        bool lootGate = false;
    };
    QVector<PlacedGate> placedGates;
    QHash<int, HiddenWall*> hiddenWallById;
    int nextWallId = 0;

    for (int i = 0; i < plans.size(); ++i) {
        if (!plans[i].active || (!plans[i].generalGate && !plans[i].lootGate)) {
            continue;
        }
        const GraphEdge& e = plans[i].edge;
        if (plans[i].gatedRoom != e.a && plans[i].gatedRoom != e.b) {
            continue;
        }
        const EdgeDoorways doors = edgeDoorwaysByPlan.value(i);
        const QPoint gateCell = (plans[i].gatedRoom == e.a) ? doors.nearA : doors.nearB;
        if (!isValidGateTile(level, gateCell)) {
            qWarning() << "Invalid gate tile for plan" << i << "at" << gateCell;
            continue;
        }

        level->setTileAt(gateCell.x(), gateCell.y(), static_cast<int>(CellType::HiddenWall));
        level->setCollisionAt(gateCell.x(), gateCell.y(), true);

        HiddenWall* hidden = new HiddenWall(gateCell.x(),
                                            gateCell.y(),
                                            nextWallId,
                                            e.a,
                                            e.b);
        level->addHiddenWall(hidden);
        hiddenWallById.insert(nextWallId, hidden);

        PlacedGate placement;
        placement.wallId = nextWallId;
        placement.planIndex = i;
        placement.gatedRoom = plans[i].gatedRoom;
        placement.gateCell = gateCell;
        placement.lootGate = plans[i].lootGate;
        placedGates.push_back(placement);

        occupied.insert(makeCellKey(gateCell.x(), gateCell.y()));
        ++nextWallId;
    }

    QJsonArray clues;
    appendCoinClues(clues, difficultyTier);

    int nextTriggerId = 0;
    auto reserveTriggerId = [&]() {
        return nextTriggerId++;
    };

    bool strictLootRoomValid = lootRoomSpawned;
    if (strictLootRoomValid) {
        const bool isLateDepth = (lootRoomNode >= 0 && zones.value(lootRoomNode, 0) == 2);
        const bool singleEntrance = (lootRoomNode >= 0 && adj.value(lootRoomNode).size() == 1);
        bool hasLootGateWall = false;
        for (const PlacedGate& gate : placedGates) {
            if (gate.lootGate && gate.gatedRoom == lootRoomNode) {
                hasLootGateWall = true;
                break;
            }
        }
        strictLootRoomValid = isLateDepth && singleEntrance && hasLootGateWall;
        if (!strictLootRoomValid) {
            qWarning() << "Loot room constraints not satisfied. Disabling loot-room specialization for this level."
                       << "lateDepth=" << isLateDepth
                       << "singleEntrance=" << singleEntrance
                       << "hasLootHiddenWall=" << hasLootGateWall;
        }
    }
    lootRoomSpawned = strictLootRoomValid;
    if (!lootRoomSpawned) {
        lootRoomNode = -1;
    }

    QVector<int> generalWallIds;
    int lootWallId = -1;
    QSet<int> gatedRooms;
    for (const PlacedGate& gate : placedGates) {
        gatedRooms.insert(gate.gatedRoom);
        if (gate.lootGate && lootRoomSpawned) {
            lootWallId = gate.wallId;
        } else {
            generalWallIds.push_back(gate.wallId);
        }
    }

    QVector<int> triggerRoomCandidates = tpl.triggerRooms;
    QVector<int> byDepth;
    for (int i = 0; i < depths.size(); ++i) {
        byDepth.push_back(i);
    }
    std::sort(byDepth.begin(), byDepth.end(), [&](int lhs, int rhs) {
        if (depths.value(lhs, -1) != depths.value(rhs, -1)) {
            return depths.value(lhs, -1) > depths.value(rhs, -1);
        }
        return lhs < rhs;
    });
    appendUnique(triggerRoomCandidates, byDepth);

    if (!generalWallIds.isEmpty()) {
        int generalTriggerCount = qMin(maxGeneralTriggers, generalWallIds.size());
        generalTriggerCount = qMax(minGeneralTriggers, generalTriggerCount);
        generalTriggerCount = qMin(generalTriggerCount, generalWallIds.size());

        QVector<int> chosenGeneralRooms;
        for (int node : triggerRoomCandidates) {
            if (chosenGeneralRooms.size() >= generalTriggerCount) {
                break;
            }
            if (node < 0 || node >= chambers.size()) {
                continue;
            }
            if (node == spawnNode || node == treasureNode || node == lootRoomNode) {
                continue;
            }
            if (!chosenGeneralRooms.contains(node)) {
                chosenGeneralRooms.push_back(node);
            }
        }
        if (chosenGeneralRooms.isEmpty() && !generalWallIds.isEmpty()) {
            for (int node = 0; node < chambers.size(); ++node) {
                if (node == lootRoomNode || node == treasureNode) {
                    continue;
                }
                chosenGeneralRooms.push_back(node);
                break;
            }
        }

        QVector<QVector<int>> wallBuckets(chosenGeneralRooms.size());
        for (int i = 0; i < generalWallIds.size(); ++i) {
            if (wallBuckets.isEmpty()) {
                break;
            }
            wallBuckets[i % wallBuckets.size()].push_back(generalWallIds.at(i));
        }

        for (int i = 0; i < chosenGeneralRooms.size(); ++i) {
            if (wallBuckets.value(i).isEmpty()) {
                continue;
            }
            const int roomIdx = chosenGeneralRooms.at(i);
            const QPoint triggerPos = pickPointInChamber(chambers[roomIdx], rng, occupied, 1);
            if (!level->isInBounds(triggerPos.x(), triggerPos.y())) {
                continue;
            }

            const int triggerId = reserveTriggerId();
            level->addTriggerWall(new TriggerWall(triggerPos.x(),
                                                  triggerPos.y(),
                                                  triggerId,
                                                  wallBuckets[i]));
            occupied.insert(makeCellKey(triggerPos.x(), triggerPos.y()));

            for (int wallId : wallBuckets[i]) {
                HiddenWall* wall = hiddenWallById.value(wallId, nullptr);
                if (wall) {
                    wall->addRequiredTriggerId(triggerId);
                }
            }

            const QPoint cluePos = pickPointInChamber(chambers[roomIdx], rng, occupied, 1);
            if (level->isInBounds(cluePos.x(), cluePos.y())) {
                const QString hintText = QStringLiteral("A pressure plate here controls one or more hidden walls.");
                level->addClueTrigger(new ClueTrigger(cluePos.x(), cluePos.y(), hintText));
                clues.append(makePositionClue(cluePos, hintText));
                occupied.insert(makeCellKey(cluePos.x(), cluePos.y()));
            }
        }
    }

    if (lootWallId >= 0 && lootRoomNode >= 0) {
        QVector<int> lootTriggerRooms;
        QVector<int> candidates = byDepth;
        const int minDistance = qMax(6, (tpl.mapW + tpl.mapH) / 8);

        for (int node : candidates) {
            if (lootTriggerRooms.size() >= lootTriggerCountRequired) {
                break;
            }
            if (node < 0 || node >= chambers.size()) {
                continue;
            }
            if (node == lootRoomNode || node == spawnNode || node == treasureNode) {
                continue;
            }
            bool farEnough = true;
            for (int picked : lootTriggerRooms) {
                const QPoint a = chambers[picked].centre();
                const QPoint b = chambers[node].centre();
                const int manhattan = qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
                if (manhattan < minDistance) {
                    farEnough = false;
                    break;
                }
            }
            if (farEnough) {
                lootTriggerRooms.push_back(node);
            }
        }
        for (int node : candidates) {
            if (lootTriggerRooms.size() >= lootTriggerCountRequired) {
                break;
            }
            if (node < 0 || node >= chambers.size()) {
                continue;
            }
            if (node == lootRoomNode || node == spawnNode || node == treasureNode) {
                continue;
            }
            if (!lootTriggerRooms.contains(node)) {
                lootTriggerRooms.push_back(node);
            }
        }

        QVector<int> linkedLootTriggers;
        for (int roomIdx : lootTriggerRooms) {
            if (linkedLootTriggers.size() >= lootTriggerCountRequired) {
                break;
            }
            const QPoint triggerPos = pickPointInChamber(chambers[roomIdx], rng, occupied, 1);
            if (!level->isInBounds(triggerPos.x(), triggerPos.y())) {
                continue;
            }
            const int triggerId = reserveTriggerId();
            level->addTriggerWall(new TriggerWall(triggerPos.x(),
                                                  triggerPos.y(),
                                                  triggerId,
                                                  QVector<int>{lootWallId}));
            linkedLootTriggers.push_back(triggerId);
            occupied.insert(makeCellKey(triggerPos.x(), triggerPos.y()));
        }
        if (linkedLootTriggers.size() < lootTriggerCountRequired) {
            for (int roomIdx : byDepth) {
                if (linkedLootTriggers.size() >= lootTriggerCountRequired) {
                    break;
                }
                if (roomIdx < 0 || roomIdx >= chambers.size()) {
                    continue;
                }
                if (roomIdx == lootRoomNode || roomIdx == spawnNode || roomIdx == treasureNode) {
                    continue;
                }
                if (lootTriggerRooms.contains(roomIdx)) {
                    continue;
                }
                const QPoint triggerPos = pickPointInChamber(chambers[roomIdx], rng, occupied, 0);
                if (!level->isInBounds(triggerPos.x(), triggerPos.y())) {
                    continue;
                }
                const int triggerId = reserveTriggerId();
                level->addTriggerWall(new TriggerWall(triggerPos.x(),
                                                      triggerPos.y(),
                                                      triggerId,
                                                      QVector<int>{lootWallId}));
                linkedLootTriggers.push_back(triggerId);
                occupied.insert(makeCellKey(triggerPos.x(), triggerPos.y()));
            }
        }

        HiddenWall* lootWall = hiddenWallById.value(lootWallId, nullptr);
        if (lootWall) {
            lootWall->setRequiredTriggerIds(linkedLootTriggers);
        }

        lootRoomSpawned = (linkedLootTriggers.size() >= lootTriggerCountRequired);
        if (!lootRoomSpawned) {
            qWarning() << "Loot room gate could not be assigned the required trigger count:"
                       << lootTriggerCountRequired << "(assigned" << linkedLootTriggers.size() << ")";
        }
    } else {
        lootRoomSpawned = false;
    }

    QVector<int> deadEnds;
    for (int i = 0; i < adj.size(); ++i) {
        if (adj[i].size() == 1) {
            deadEnds.push_back(i);
        }
    }
    QVector<int> lateNodes;
    for (int i = 0; i < zones.size(); ++i) {
        if (zones[i] == 2) {
            lateNodes.push_back(i);
        }
    }
    std::sort(lateNodes.begin(), lateNodes.end(), [&](int lhs, int rhs) {
        return depths.value(lhs, -1) > depths.value(rhs, -1);
    });

    QVector<int> coinPriority;
    appendUnique(coinPriority, tpl.rewardDeadEnds);
    appendUnique(coinPriority, deadEnds);
    appendUnique(coinPriority, tpl.riskyNodes);
    appendUnique(coinPriority, lateNodes);
    for (int i = 0; i < chambers.size(); ++i) {
        appendUnique(coinPriority, i);
    }

    int coinsPlaced = 0;
    for (int node : coinPriority) {
        if (coinsPlaced >= tpl.coinTarget) {
            break;
        }
        if (node == spawnNode || node == treasureNode || node == lootRoomNode) {
            continue;
        }
        if (node < 0 || node >= chambers.size()) {
            continue;
        }

        const QPoint p = pickPointInChamber(chambers[node], rng, occupied, 1);
        if (!level->isInBounds(p.x(), p.y())) {
            continue;
        }
        level->addCoin(new Coin(p.x(), p.y(), 100));
        occupied.insert(makeCellKey(p.x(), p.y()));
        ++coinsPlaced;
    }

    if (lootRoomSpawned && lootRoomNode >= 0) {
        QVector<QPoint> lootCandidates;
        const Chamber& lootChamber = chambers[lootRoomNode];
        for (int y = lootChamber.y + 1; y < lootChamber.y + lootChamber.height - 1; ++y) {
            for (int x = lootChamber.x + 1; x < lootChamber.x + lootChamber.width - 1; ++x) {
                const quint64 key = makeCellKey(x, y);
                if (!occupied.contains(key)) {
                    lootCandidates.push_back(QPoint(x, y));
                }
            }
        }
        for (int i = lootCandidates.size() - 1; i > 0; --i) {
            const int j = rng.bounded(i + 1);
            std::swap(lootCandidates[i], lootCandidates[j]);
        }
        if (lootCandidates.size() < 3) {
            qWarning() << "Loot room selected but insufficient floor cells for exactly 3 coins.";
        }
        const int lootCoins = qMin(3, lootCandidates.size());
        for (int i = 0; i < lootCoins; ++i) {
            const QPoint p = lootCandidates.at(i);
            level->addCoin(new Coin(p.x(), p.y(), 100));
            occupied.insert(makeCellKey(p.x(), p.y()));
        }
    }

    QVector<int> junctionNodes;
    for (int i = 0; i < adj.size(); ++i) {
        if (adj[i].size() >= 3) {
            junctionNodes.push_back(i);
        }
    }
    std::sort(junctionNodes.begin(), junctionNodes.end(), [&](int lhs, int rhs) {
        if (zones.value(lhs, 0) != zones.value(rhs, 0)) {
            return zones.value(lhs, 0) > zones.value(rhs, 0);
        }
        return adj.value(lhs).size() > adj.value(rhs).size();
    });

    QVector<int> enemyPriority;
    appendUnique(enemyPriority, junctionNodes);
    appendUnique(enemyPriority, tpl.trapDeadEnds);
    appendUnique(enemyPriority, tpl.riskyNodes);
    appendUnique(enemyPriority, lateNodes);
    for (int i = 0; i < chambers.size(); ++i) {
        appendUnique(enemyPriority, i);
    }

    int enemiesPlaced = 0;
    for (int node : enemyPriority) {
        if (enemiesPlaced >= tpl.enemyTarget) {
            break;
        }
        if (node == spawnNode || node == treasureNode || node == lootRoomNode) {
            continue;
        }
        if (node < 0 || node >= chambers.size()) {
            continue;
        }

        const QPoint p = pickPointInChamber(chambers[node], rng, occupied, 1);
        if (!level->isInBounds(p.x(), p.y())) {
            continue;
        }
        level->addEnemy(new Enemy(p.x(), p.y()));
        occupied.insert(makeCellKey(p.x(), p.y()));
        ++enemiesPlaced;
    }

    for (int gatedRoom : gatedRooms) {
        const int degree = adj.value(gatedRoom).size();
        if (degree != 1) {
            qWarning() << "Graph integrity failed: gated room" << gatedRoom
                       << "has degree" << degree << "(expected 1)";
        }
    }
    for (int node = 0; node < depths.size(); ++node) {
        if (depths[node] < 0) {
            qWarning() << "Graph integrity warning: node" << node << "is unreachable from spawn";
        }
    }

    level->validateTriggerWallConsistency();

    if (outLootRoomSpawned) {
        *outLootRoomSpawned = lootRoomSpawned;
    }
    if (outClues) {
        *outClues = clues;
    }
    return level;
}

QStringList LevelLoader::getAvailableLevels(const QString& levelsDir)
{
    QDir dir(levelsDir);
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
    QStringList result;
    result.reserve(files.size());
    for (const QString& file : files) {
        const QString absPath = dir.absoluteFilePath(file);
        QFile levelFile(absPath);
        if (!levelFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(levelFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }
        const QJsonObject root = doc.object();
        const bool isChamberLevel = root.value("format").toString() == "chambers"
                                    && root.value("chambers").isArray()
                                    && root.value("corridors").isArray();
        const bool isLegacyLevel = root.value("width").isDouble()
                                   && root.value("height").isDouble()
                                   && (root.value("tiles").isArray()
                                       || root.value("walls").isArray()
                                       || root.value("hiddenWalls").isArray()
                                       || root.value("triggerWalls").isArray());
        if (isChamberLevel || isLegacyLevel) {
            result.push_back(absPath);
        }
    }
    return result;
}
