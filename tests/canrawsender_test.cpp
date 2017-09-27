#include <QJsonArray>
#include <QJsonDocument>
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

TEST_CASE("CanRawSender, setConfig test", "[CanRawSender]")
{
    using namespace fakeit;
    auto crsMock = getCrsMock();

    Mock<NLMFactoryInterface> nlmFactoryMock;
    Fake(Dtor(nlmFactoryMock));

    CanRawSender canRawSender{ CanRawSenderCtx(&crsMock.get(), &nlmFactoryMock.get()) };
    canRawSender.setConfig(QJsonObject());
    CHECK(canRawSender.getConfig().isEmpty() == false);
}
