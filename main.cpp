#include <QFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

#define ROOT "/home/bardia/Projects/Aseman/Apps/Meikade"

template<typename T>
T insertImportsAtLast(const T &d, const T &import_text)
{
    auto lines = d.split('\n');
    T data;
    bool imports_started = false;
    bool privates_added = false;
    for (const auto &l: lines)
    {
        if (l.left(6) == "import")
            imports_started = true;
        if (!imports_started)
        {
            data += l + "\n";
            continue;
        }

        if (!privates_added && l.left(6) != "import")
        {
            data += import_text;
            privates_added = true;
        }

        data += l + "\n";
    }

    return data;
}

void moveRecursive(const QString &source, const QString &dest)
{
    QDir().mkpath(dest);
    for (const auto &f: QDir(source).entryList(QDir::Files))
        QFile::rename(source + '/' + f, dest + '/' + f);
    for (const auto &d: QDir(source).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        moveRecursive(source + '/' + d, dest + '/' + d);
    QDir(source).removeRecursively();
}

QStringList getFilesList(const QString &dir, const QString &path = QString())
{
    QStringList res;
    for (const auto &f: QDir(dir + '/' + path).entryList(QDir::Files))
        res << path + f;
    for (const auto &d: QDir(dir + '/' + path).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        res << getFilesList(dir, path + d + '/');
    return res;
}

void moveViewsToPrivates(const QString &dir, const QString &new_dir, const QString &name_space, int level = 0)
{
    QDir().mkpath(dir);
    QDir().mkpath(new_dir);

    auto files = QDir(dir).entryList(QDir::Files);

    for (const auto &f: files)
    {
        if (f.right(4) == ".qml")
        {
            QDir().mkpath(new_dir + '/' + name_space + "/privates");
            QString new_f = QString(f).replace(".ui.qml", ".qml");
            QFile::rename(dir + '/' + f, new_dir + '/' + name_space + "/privates/" + new_f);
            qDebug() << dir + '/' + f << new_dir + '/' + name_space + "/privates/" + new_f;
        }
        else
        {
            QDir().mkpath(new_dir + '/' + name_space);
            QFile::rename(dir + '/' + f, new_dir + '/' + name_space + '/' + f);
            qDebug() << dir + '/' + f << new_dir + '/' + name_space + '/' + f;
        }
    }

    auto sub_dirs = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &d: sub_dirs)
    {
        if (level == 1)
            moveViewsToPrivates(dir + '/' + d, new_dir + '/' + name_space + '/' + d, QString(), level + 1);
        else
            moveViewsToPrivates(dir + '/' + d, new_dir + '/' + d, name_space, level + 1);
    }
}

void fixViewPathsAndHeaders(const QString &dir)
{
    auto sub_dirs = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &d: sub_dirs)
        fixViewPathsAndHeaders(dir + '/' + d);

    if (dir.contains("privates"))
    {
        auto files = QDir(dir).entryList({"*.qml"}, QDir::Files);
        auto list = getFilesList(dir + "/..");

        for (const QString &fn: files)
        {
            QFile f(dir + '/' + fn);
            if (!f.open(QFile::ReadOnly))
                continue;

            QString data = QString::fromUtf8(f.readAll());
            f.close();

            int count = 0;
            for (const auto &l: list)
            {
                if (l.contains(".qml"))
                    continue;

                QStringList modes;
                modes << l;
                if (l.contains('.'))
                    modes << l.left( l.lastIndexOf('.') );

                for (const auto &m: modes)
                {
                    int cnt = data.count("\"" + m + "\"");
                    if (cnt)
                        data.replace("\"" + m + "\"", "\"../" + l + "\"");

                    count += cnt;
                }
            }

            data.replace(QByteArray("import micros 1.0\n"), QByteArray("import components 1.0\n"));

            f.open(QFile::WriteOnly);
            f.write(data.toUtf8());
            f.close();
            qDebug() << count << "paths replaced in" << fn;
        }
    }
    else
    {
        if (!sub_dirs.contains("privates"))
            return;

        auto files = QDir(dir).entryList({"*.qml"}, QDir::Files);
        for (const auto &fn: files)
        {
            QFile f(dir + '/' + fn);
            f.open(QFile::ReadOnly);
            auto d = f.readAll();
            f.close();

            d.replace(QByteArray("import micros 1.0\n"), QByteArray("import components 1.0\n"));

            f.open(QFile::WriteOnly);
            f.write( insertImportsAtLast<QByteArray>(d, "import \"privates\"\n") );
            f.close();
        }
    }
}

void moveToNewPlace(const QString &root, const QString &name_space)
{
    QFile f(root + "/qmldir");
    if (!f.open(QFile::ReadOnly))
    {
        qDebug() << "Can't read view's qmldir";
        return;
    }

    auto lines = QString::fromUtf8(f.readAll()).split("\n", Qt::SkipEmptyParts);

    for (const auto &l: lines)
    {
        auto parts = l.split(QRegularExpression("\\s+"));
        if (parts.count() < 3)
            continue;
        if (parts.first() == "singleton")
            parts.takeFirst();

        const auto new_name = parts.at(0) + ".qml";
        const auto file = parts.at(2);

        QString path;
        if (file.contains('/'))
            path = file.left(file.indexOf('/'));

        QString new_path = "../new/" + path + '/' + name_space;

        QString src_file = root + '/' + file;
        if (!QFileInfo::exists(src_file))
            continue;

        QString dst_file = root + '/' + new_path + '/' + new_name;
        QDir(root).mkpath(new_path);

        if (!QFile::rename(src_file, dst_file))
            qDebug() << "Copy" << src_file << "to" << dst_file;
        else
            qDebug() << "Error to copy" << src_file << "to" << dst_file;
    }
}

void moveViews()
{
    moveToNewPlace(ROOT "/qml/imports/views", "views");
    moveViewsToPrivates(ROOT "/qml/imports/views", ROOT "/qml/imports/new", "views");
    fixViewPathsAndHeaders(ROOT "/qml/imports/new");
    QDir(ROOT "/qml/imports/views").removeRecursively();
}

void moveLogics()
{
    moveRecursive(ROOT "/qml/imports/logics/files", ROOT "/qml/imports/new/files");
    moveRecursive(ROOT "/qml/imports/logics/old", ROOT "/qml/imports/new/old");

    const auto views = getFilesList(ROOT "/qml/imports/new");
    QMap<QString, QString> views_map;
    for (const auto &v: views)
    {
        if (v.right(4) != ".qml")
            continue;

        QFileInfo inf(ROOT + v);
        views_map[inf.baseName()] = v;
    }

    struct QmlFile {
        QString path;
        QString data;
        QString filename;
    };

    QMap<QString, QmlFile> components;

    QString root = ROOT "/qml/imports/logics";
    const auto qmls = QDir(root).entryList({"*.qml"}, QDir::Files);
    for (const auto &fn: qmls)
    {
        QFile f(root + '/' + fn);
        if (!f.open(QFile::ReadOnly))
            continue;

        const auto data = QString::fromUtf8(f.readAll());
        f.close();

        QmlFile file;
        file.data = data;
        file.filename = fn;

        if (data.contains("import views 1.0"))
        {
            QStringList used_views;
            for (const auto &component: views_map.keys())
                if (data.contains(QRegularExpression("[\n\\s]" + component + "\\s*\\{")))
                    used_views << views_map.value(component);

            QString imports = "import \"views\"\n";
            while (used_views.count())
            {
                const auto v = used_views.takeFirst();
                if (v.contains("/views"))
                {
                    auto view = v.left(v.indexOf("/views"));
                    if (file.path.isEmpty())
                        file.path = view;
                    else if (file.path != view && !imports.contains("../" + view))
                        imports += "import \"../" + view + "/views\"\n";
                }
            }

            qDebug() << fn << file.path << imports;

            file.data.remove("import views 1.0\n");
            file.data = insertImportsAtLast<QString>(file.data, imports);
        }

        components[ QString(fn).remove(".qml") ] = file;
    }

    for (auto &cmp: components)
    {
        QSet<QString> imports;
        for (const auto &k: components.keys())
        {
            if (!cmp.data.contains(k))
                continue;

            const auto &path = components.value(k).path;
            if (path == cmp.path)
                continue;

            imports.insert(path);
        }

        QString import;
        for (const auto &i: imports)
            import += "import \"" + QString(cmp.path.isEmpty()? "" : "../") + i + "\"\n";

        cmp.data = insertImportsAtLast<QString>(cmp.data, import);

        qDebug() << cmp.filename << cmp.path << import;
        qDebug() << ROOT "/qml/imports/new/" + cmp.path + '/' + cmp.filename;

        QFile f(ROOT "/qml/imports/new/" + cmp.path + '/' + cmp.filename);
        if (!f.open(QFile::WriteOnly))
            continue;

        f.write(cmp.data.toUtf8());
        f.close();

        QFile::remove(root + '/' + cmp.filename);
    }

    QDir(root).removeRecursively();
}

void moveRoutes()
{
    QMap<QString, QString> converteds;

    const auto files = getFilesList(ROOT "/qml/imports/new");
    QMap<QString, QString> files_map;
    for (const auto &v: files)
    {
        if (v.right(4) != ".qml")
            continue;
        if (v.contains("imports"))
            continue;

        QFileInfo inf(ROOT + v);
        files_map[inf.baseName()] = v;
    }

    struct QmlFile {
        QString path;
        QString data;
        QString filename;
    };

    QString root = ROOT "/qml/imports/routes";
    const auto qmls = QDir(root).entryList({"*.qml"}, QDir::Files);
    for (const auto &fn: qmls)
    {
        if (fn == "WaitDialogRoute.qml" ||
            fn == "ViewController.qml")
            continue;

        QFile f(root + '/' + fn);
        if (!f.open(QFile::ReadOnly))
            continue;

        const auto data = QString::fromUtf8(f.readAll());
        f.close();

        QmlFile file;
        file.data = data;
        file.filename = fn;

        if (data.contains("import logics 1.0"))
        {
            QStringList used_logics;
            for (const auto &component: files_map.keys())
                if (data.contains(QRegularExpression("[\n\\s]" + component + "\\s*\\{")))
                    used_logics << files_map.value(component);

            QString imports = "import \"..\"\n";
            while (used_logics.count())
            {
                const auto v = used_logics.takeFirst();
                if (v.contains("/"))
                {
                    auto logic = v.left(v.indexOf("/"));
                    if (file.path.isEmpty())
                        file.path = logic;
                    else if (file.path != logic && !imports.contains("../" + logic))
                        imports += "import \"../../" + logic + "\"\n";
                }
            }

            file.data.remove("import logics 1.0\n");
            file.data = insertImportsAtLast<QString>(file.data, imports);
        }

        converteds[fn] = file.path + "/routes/" + fn;

        QDir().mkpath(ROOT "/qml/imports/new/" + file.path + "/routes/");

        f.remove();
        f.setFileName(ROOT "/qml/imports/new/" + file.path + "/routes/" + file.filename);
        f.open(QFile::WriteOnly);
        f.write(file.data.toUtf8());
        f.close();
    }

    QDir().mkpath(ROOT "/qml/imports/new/imports/routes");
    QFile::rename(ROOT "/qml/imports/routes/qmldir", ROOT "/qml/imports/new/imports/routes/qmldir");

    QFile f(ROOT "/qml/imports/routes/WaitDialogRoute.qml");
    f.open(QFile::ReadOnly);
    auto d = f.readAll();
    d.replace("import micros", "import components");
    f.close();
    f.remove();

    f.setFileName(ROOT "/qml/imports/new/imports/routes/WaitDialogRoute.qml");
    f.open(QFile::WriteOnly);
    f.write(d);
    f.close();

    f.setFileName(ROOT "/qml/imports/routes/ViewController.qml");
    f.open(QFile::ReadOnly);
    d = f.readAll();
    d.replace("import logics 1.0\n", "");
    for (const auto &[k, v]: converteds.toStdMap())
        d.replace(k.toUtf8(), ("qrc:/" + v).toUtf8());

    f.close();
    f.remove();

    f.setFileName(ROOT "/qml/imports/new/imports/routes/ViewController.qml");
    f.open(QFile::WriteOnly);
    f.write(d);
    f.close();

    QDir(root).removeRecursively();
}

void moveImports()
{
    moveRecursive(ROOT "/qml/imports/micros/", ROOT "/qml/imports/new/imports/components/");
    moveRecursive(ROOT "/qml/imports/globals/", ROOT "/qml/imports/new/imports/globals/");
    moveRecursive(ROOT "/qml/imports/models/", ROOT "/qml/imports/new/imports/models/");
    moveRecursive(ROOT "/qml/imports/queries/", ROOT "/qml/imports/new/imports/queries/");
    moveRecursive(ROOT "/qml/imports/requests/", ROOT "/qml/imports/new/imports/requests/");
}

int main()
{
    moveViews();
    moveLogics();
    moveRoutes();
    moveImports();
    moveRecursive(ROOT "/qml/imports/new/", ROOT "/qml/");
    return 0;
}
