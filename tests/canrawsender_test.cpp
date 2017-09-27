#include <QJsonArray>
#include <QJsonDocument>
#include <QByteArray>
#include <QStandardItemModel>
#include <canrawsender.h>
#include <context.h>
#include <fakeit.hpp>
#include <gui/crsguiinterface.h>
#include <memory>
#include <newlinemanager.h>

namespace {
fakeit::Mock<CRSGuiInterface> getCrsMock()
{
    using namespace fakeit;

    Mock<CRSGuiInterface> crsMock;
    Fake(Dtor(crsMock));
    Fake(Method(crsMock, setAddCbk));
    Fake(Method(crsMock, setRemoveCbk));
    Fake(Method(crsMock, setDockUndockCbk));
    Fake(Method(crsMock, mainWidget));
    Fake(Method(crsMock, initTableView));
    Fake(Method(crsMock, getSelectedRows));
    Fake(Method(crsMock, setIndexWidget));
    return crsMock;
}
}

TEST_CASE("Add and remove frame test", "[canrawsender]")
{
    class helpTestClass {
    public:
        helpTestClass()
        {
            list.append(&item);
            tvModel.appendRow(list);
            auto idx = tvModel.indexFromItem(list.at(0));
            indexList.append(idx);
        }
        QModelIndexList getList()
        {
            return indexList;
        }

    private:
        QStandardItemModel tvModel;
        QStandardItem item;
        QList<QStandardItem*> list;
        QModelIndexList indexList;
    };

    using namespace fakeit;
    CRSGuiInterface::add_t addLineCbk;
    CRSGuiInterface::remove_t removeLineCbk;

    Mock<CheckBoxInterface> nlmCheckBoxMock;
    Mock<LineEditInterface> nlmLineEditMock;
    Mock<PushButtonInterface> nlmPushButtonMock;

    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));
    helpTestClass mHelp;

    Fake(Dtor(nlmLineEditMock));
    Fake(Method(nlmLineEditMock, textChangedCbk));
    Fake(Method(nlmLineEditMock, init));
    Fake(Method(nlmLineEditMock, setPlaceholderText));
    Fake(Method(nlmLineEditMock, setDisabled));
    When(Method(nlmLineEditMock, mainWidget)).AlwaysDo([&]() {
        return reinterpret_cast<QWidget*>(&nlmLineEditMock.get());
    });
    When(Method(nlmFactoryMock, createLineEdit)).AlwaysDo([&]() { return &nlmLineEditMock.get(); });

    Fake(Dtor(nlmCheckBoxMock));
    Fake(Method(nlmCheckBoxMock, releasedCbk));
    When(Method(nlmCheckBoxMock, mainWidget)).Return(reinterpret_cast<QWidget*>(&nlmCheckBoxMock.get()));
    When(Method(nlmFactoryMock, createCheckBox)).Return(&nlmCheckBoxMock.get());

    Fake(Dtor(nlmPushButtonMock));
    Fake(Method(nlmPushButtonMock, init));
    Fake(Method(nlmPushButtonMock, pressedCbk));
    When(Method(nlmPushButtonMock, mainWidget)).Return(reinterpret_cast<QWidget*>(&nlmPushButtonMock.get()));
    When(Method(nlmFactoryMock, createPushButton)).Return(&nlmPushButtonMock.get());

    auto crsMock = getCrsMock();
    When(Method(crsMock, setAddCbk)).Do([&](auto&& fn) { addLineCbk = fn; });
    When(Method(crsMock, setRemoveCbk)).Do([&](auto&& fn) { removeLineCbk = fn; });
    When(Method(crsMock, getSelectedRows)).Do([&]() { return mHelp.getList(); });

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };

    CHECK(canRawSender.getLineCount() == 0);
    addLineCbk();
    CHECK(canRawSender.getLineCount() == 1);
    removeLineCbk();
    CHECK(canRawSender.getLineCount() == 0);
}

TEST_CASE("Can raw sender save configuration test", "[newlinemanager]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();

    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };

    QJsonObject json = canRawSender.getConfig();

    CHECK(json.empty() == false);
    CHECK(json.count() == 3);
    const auto colIter = json.find("columns");
    CHECK(colIter != json.end());
    CHECK(colIter.value().type() == QJsonValue::Array);
    const auto colArray = json["columns"].toArray();
    CHECK(colArray.empty() == false);
    CHECK(colArray.size() == 5);
    CHECK(colArray.contains("Id") == true);
    CHECK(colArray.contains("Data") == true);
    CHECK(colArray.contains("Loop") == true);
    CHECK(colArray.contains("Interval") == true);

    CHECK(json.contains("content") == true);

    const auto sortIter = json.find("sorting");
    CHECK(sortIter != json.end());
    CHECK(sortIter.value().type() == QJsonValue::Object);
    const auto sortObj = json["sorting"].toObject();
    CHECK(sortObj.contains("currentIndex") == true);
}

TEST_CASE("CanRawSender, columnAdopt, Columns item not found it", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
     
    CHECK(canRawSender.setConfig(QJsonObject()) == false);
}

TEST_CASE("CanRawSender, columnAdopt, Columns format is different then array", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonValue());

    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, columnAdopt, Columns array size must be 5", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray());

    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, columnAdopt, Columns array does not contain Id field", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"IdWrong", "Data", "Loop", "Interval", ""});

    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, columnAdopt, Columns array does not contain Data field", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "DataWrong", "Loop", "Interval", ""});

    CHECK(canRawSender.setConfig(jsonObject) == false);
}


TEST_CASE("CanRawSender, columnAdopt, Columns array does not contain Loop field", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "LoopWrong", "Interval", ""});

    CHECK(canRawSender.setConfig(jsonObject) == false);
}


TEST_CASE("CanRawSender, columnAdopt, Columns array does not contain Interval field", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "IntervalWrong", ""});

    CHECK(canRawSender.setConfig(jsonObject) == false);
}


TEST_CASE("CanRawSender, contentAdopt, content item not found", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});

    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, contentAdopt, content format is different than array", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonValue());
    CHECK(canRawSender.setConfig(jsonObject) == false);
}


TEST_CASE("CanRawSender, contentAdopt, sorting item not found", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonArray());
    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, sortingAdopt, sorting format is different then object", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonArray());
    jsonObject.insert("sorting", QJsonValue());
    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, sortingAdopt, object count is different than 1", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonArray());
    jsonObject.insert("sorting", QJsonObject());
    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, sortingAdopt, sorting object does not contain currentIndex", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonArray());
    jsonObject.insert("sorting", QJsonObject{{"", QJsonValue()}});
    CHECK(canRawSender.setConfig(jsonObject) == false);
}

TEST_CASE("CanRawSender, setConfig, successfull validation", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();
    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    auto jsonObject = QJsonObject();
    jsonObject.insert("columns", QJsonArray{"Id", "Data", "Loop", "Interval", ""});
    jsonObject.insert("content", QJsonArray());
    jsonObject.insert("sorting", QJsonObject{{"currentIndex", QJsonValue(1)}});
    CHECK(canRawSender.setConfig(jsonObject) == true);
}
