import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared/sharedTests.js" as SharedTests

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized : false

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            // Gecko does not switch to SW mode if gl context failed to init
            // and qmlmoztestrunner does not build in GL mode
            // Let's put it here for now in SW mode always
            mozContext.instance.setIsAccelerated(true);
        }
        onRecvObserve: {
            print("onRecvObserve: msg:", message, ", data:", data.data);
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        anchors.fill: parent
        Connections {
            target: webViewport.child
            onViewInitialized: {
                webViewport.child.loadFrameScript("chrome://embedlite/content/embedhelper.js");
                webViewport.child.loadFrameScript("chrome://embedlite/content/SelectHelper.js");
                webViewport.child.useQmlMouse = true;
                appWindow.mozViewInitialized = true
            }
        }
    }

    MouseArea {
        id: viewportMouse
        anchors.fill: parent
        onPressed: {
            webViewport.child.recvMousePress(mouseX, mouseY)
        }
        onReleased: {
            webViewport.child.recvMouseRelease(mouseX, mouseY)
        }
        onPositionChanged: {
            webViewport.child.recvMouseMove(mouseX, mouseY)
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_inputtest cleanup")
        }

        function test_ActiveHyperLink()
        {
            SharedTests.shared_ActiveHyperLink();
            webViewport.child.useQmlMouse = false;
        }
    }
}
