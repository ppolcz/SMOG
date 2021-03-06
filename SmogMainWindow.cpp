#include "SmogMainWindow.hpp"
#include "ui_SmogMainWindow.h"
// Qt
#include <QFileDialog>
#include <QColorDialog>
#include <QSettings>
// Pcl
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/impl/instantiate.hpp>
#include <pcl/visualization/impl/pcl_visualizer.hpp>
// Std
#include <iostream>
// Backend
#include "CloudStore.hpp"
#include "AdaptiveCloudEntry.hpp"
#include "PcdCloudData.hpp"
#include "LasCloudData.hpp"
#include "CacheDatabase.hpp"
// Tools
#include "PclCameraWrapper.hpp"

/**
 * Constructor of the main window.
 * @brief SmogMainWindow constructor.
 * @param parent the parend widget of the window.
 */
SmogMainWindow::SmogMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SmogMainWindow)
{
    // Set organization name
    QCoreApplication::setOrganizationName("PPKE-ITK");
    // Set organization domain
    QCoreApplication::setOrganizationDomain("itk.ppke.hu");
    // Set application name
    QCoreApplication::setApplicationName("Smog");
    // Set application version
    QCoreApplication::setApplicationVersion("0.0.1");
    // Setup the ui
    ui->setupUi(this);
    // Create model
    mCloudModel.reset(new CloudModel(&CloudStore::getInstance()));
    // Set model
    ui->CloudList->setModel(mCloudModel.get());
    // Register visualizer events
    ui->CloudVisualizer->visualizer().registerMouseCallback(&SmogMainWindow::onVisualizerMouse, *this, this);
    ui->CloudVisualizer->visualizer().registerKeyboardCallback(&SmogMainWindow::onVisualizerKeyboard, *this, this);
    // Connent model to window
    connect(mCloudModel.get(), SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(cloudModelChanged(const QModelIndex&, const QModelIndex&)));
    // Open cache database
    CacheDatabase::getInstance().openDB();
    // Prepare cache database
    CacheDatabase::getInstance().prepareDB();
    // Start maximized
    showMaximized();
    // Create tmp folder
    QDir dir("tmp");
    if(!dir.exists())
        dir.mkpath(".");
    // Default value for use cache
    AdaptiveCloudEntry::UseCache = ui->actionUse_cache->isChecked();

    /*
    // Test cloud for Bigyo
    typedef QMapWidget::PointCloud::PointT PointT;
    auto cloud = ui->Map->getCloud("Teszt");
    for(float x = 100; x < 200; x += 20)
        for(float y = 100; y < 200; y += 20)
            for(float z = 0; z < 200; z += 20)
                cloud->points.push_back(PointT(x+z,y+2*z,z));
    ui->Map->cameraToClouds();
    //*/
}

/**
 * Destructor of the main window.
 */
SmogMainWindow::~SmogMainWindow()
{
    // Delete user interface.
    delete ui;
}

/**
 * @brief Called when the load action's triggered
 */
void SmogMainWindow::on_actionLoad_Cloud_triggered()
{
    // Settings object
    QSettings settings;
    // QString selected filter
    QString filetype;
    // Get file path to load
    QString filename = QFileDialog::getOpenFileName(this, "Load file", settings.value("main/lastdir", "").toString(), "Point cloud(*.pcd *.las);;All files(*)", &filetype);
    // Load
    loadCloudFromFile(filename);
}

void SmogMainWindow::on_actionIncrease_point_size_triggered()
{
    // Increase with 1
    changeSelectedCloudsPointSize(1);
}

void SmogMainWindow::on_actionDecrease_point_size_triggered()
{
    // Decrease with 1
    changeSelectedCloudsPointSize(-1);
}

void SmogMainWindow::on_actionBackground_Color_triggered()
{
    // Color picker
    QColor newBackgroundColor = QColorDialog::getColor(Qt::black, this);
    // If selected
    if(newBackgroundColor.isValid())
    {
        // Set background color
        ui->CloudVisualizer->visualizer().setBackgroundColor(newBackgroundColor.redF(), newBackgroundColor.greenF(), newBackgroundColor.blueF());
        // Settings
        QSettings settings;
        // Set to settings
        settings.setValue("visualizer/bgcolor", newBackgroundColor);
    }
}

void SmogMainWindow::cloudModelChanged(const QModelIndex &from, const QModelIndex &to)
{
    Q_UNUSED(to);
    // Get corresponding cloud
    auto& cloud = CloudStore::getInstance().getCloud(from.row());
    // Log name
    QTextStream out(stdout);
    out << "Cloud name: " << cloud->getName() << " Set to " << (cloud->isVisible()) << '\n';
    // Update cloud on visualizer
    if(from.column() == CloudModel::COLUMN_VISIBILITY)
        updateOnVisibility(cloud);
}



void SmogMainWindow::updateOnVisibility(CloudEntry::Ptr cloudEntry)
{
    // Call visualize
    cloudEntry->visualize(&ui->CloudVisualizer->visualizer(), ui->Map);
    // Update visualizer
    ui->CloudVisualizer->update();
}

void SmogMainWindow::changeCloudPointSize(CloudEntry::Ptr cloud, int pointSizeDiff)
{
    // Size
    double pointSize = 1.0f;
    // Get size
    ui->CloudVisualizer->visualizer().getPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, pointSize, cloud->getName().toStdString());
    // Change
    pointSize += pointSizeDiff;
    // Increase and set
    ui->CloudVisualizer->visualizer().setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, pointSize, cloud->getName().toStdString());
    // Update
    ui->CloudVisualizer->update();
}

void SmogMainWindow::changeSelectedCloudsPointSize(int pointSizeDiff)
{
    // For all selected row
    foreach (QModelIndex index, ui->CloudList->selectionModel()->selectedRows())
    {
        // Change cloud point size
        changeCloudPointSize(CloudStore::getInstance().getCloud(index.row()), pointSizeDiff);
    }
}

void SmogMainWindow::onVisualizerMouse(const pcl::visualization::MouseEvent &, void *)
{
    if(!ui->actionLock_adaptive_view->isChecked())
    {
        for(size_t i = 0; i < CloudStore::getInstance().getNumberOfClouds(); ++i)
        {
            AdaptiveCloudEntry* cloudEntry = dynamic_cast<AdaptiveCloudEntry*>(CloudStore::getInstance().getCloud(i).get());
            if(cloudEntry)
            {
                cloudEntry->updateVisualization(&ui->CloudVisualizer->visualizer(), ui->Map);
            }
        }
    }
}

void SmogMainWindow::onVisualizerKeyboard(const pcl::visualization::KeyboardEvent &, void *)
{
}

void SmogMainWindow::loadCloudFromFile(const QString &filepath)
{
    // If valid
    if(!filepath.isNull())
    {
        // Settings object
        QSettings settings;
        // Logger
        QTextStream out(stdout);
        // Create file info object
        QFileInfo fileinfo(filepath);
        // Log selected file
        out << "[Main] Load file: dir: " << fileinfo.dir().absolutePath() << ", name: " << fileinfo.fileName() << ", extension: " << fileinfo.suffix() << '\n';
        // Cloud store
        auto& cloudStore = CloudStore::getInstance();
        // Add cloud
        mCloudModel->addCloud(fileinfo.baseName(), fileinfo.absoluteFilePath(), ui->actionUse_adaptive_clouds->isChecked());
        // Update viz
        updateOnVisibility(cloudStore.getCloud(cloudStore.getNumberOfClouds() - 1));
        // Set last used directory
        settings.setValue("main/lastdir", fileinfo.dir().absolutePath());
    }
}

void SmogMainWindow::on_actionClose_Cloud_triggered()
{
    // Deleted items
    int deleted = 0;
    // Get selected clouds
    foreach (QModelIndex index, ui->CloudList->selectionModel()->selectedRows())
    {
        // Get cloud
        auto& cloud = CloudStore::getInstance().getCloud(index.row() - deleted);
        // Remove cloud from visualizer
        cloud->setVisible(false);
        updateOnVisibility(cloud);
        // Remove the cloud
        mCloudModel->removeCloud(index.row() - deleted);
        // Inc deleted
        ++deleted;
    }
}

void SmogMainWindow::on_actionCut_out_Subcloud_triggered()
{
    // Settings object
    QSettings settings;
    // Open save dialog
    QString filepath = QFileDialog::getSaveFileName(this,"Save filtered file", settings.value("main/lastdir", "").toString(), "*.las");
    // If not null
    if(!filepath.isNull())
    {
        // Get polygon
        const math::Polygonf& polygon = ui->Map->getKnifePolygon();
        // If polygon is't big enought or not simple, do nothing
        if(polygon.size() < 3 || !math::isPolygonSimple(polygon))
            return;
        // Filter
        CloudStore::getInstance().filterVisibleCloudsTo(ui->Map->getKnifePolygon(),filepath);
        // Load
        bool tmp = AdaptiveCloudEntry::UseCache;
        AdaptiveCloudEntry::UseCache = false;
        loadCloudFromFile(filepath);
        AdaptiveCloudEntry::UseCache = tmp;
    }
}

void SmogMainWindow::on_actionUse_cache_triggered()
{
    // Update use cache
    AdaptiveCloudEntry::UseCache = ui->actionUse_cache->isChecked();
}
