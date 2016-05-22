<?php

/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

namespace OMV\Engined\Rpc;

use DateTime;
use Exception;
use OMV\Config\Database;
use OMV\Engine\Notify\Dispatcher;
use OMV\Rpc\ServiceAbstract;
use OMV\Rpc\ServiceManager as RpcServiceManager;
use OMV\System\Process;
use SplFileObject;

class JDownloader extends ServiceAbstract
{
    private $database;

    private $dataModelPath = 'conf.service.jdownloader';

    private $dispatcher;

    private $eventMessagePath = 'org.openmediavault.conf.services.jdownloader';

    private $jdownloaderSettingsFile = '/opt/jdownloader/cfg/org.jdownloader.api.myjdownloader.MyJDownloaderSettings.json';

    private $jdownloaderLocationFile = '/opt/jdownloader/cfg/org.jdownloader.settings.GeneralSettings.json';

    private $jdownloaderBuildFile = '/opt/jdownloader/build.json';

    private $jdownloaderServiceFile = '/lib/systemd/system/jdownloader.service';

    public function __construct(Database $database = null, Dispatcher $dispatcher = null)
    {
        $this->database = $database ?: Database::getInstance();
        $this->dispatcher = $dispatcher ?: Dispatcher::getInstance();
    }

    public function getName()
    {
        return 'JDownloader';
    }

    public function initialize()
    {
        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
    }

    public function getSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $object = $this->database->get($this->dataModelPath);

        $jdbuilddate = 'awaiting info';
        $jdversion   = 'awaiting info';
        if (file_exists($this->jdownloaderBuildFile)) {
            $string       = file_get_contents($this->jdownloaderBuildFile);
            $json_c       = json_decode($string,true);
            $jdbuilddate  = $json_c['buildDate'];
            $jdversion    = $json_c['JDownloaderRevision'];
        }

        // Get current info as set in Jdownloader config file
        $uniquedeviceid = 'awaiting info';
        $jderror        = 'awaiting info';
        if (file_exists($this->jdownloaderSettingsFile)) {
            $string = file_get_contents($this->jdownloaderSettingsFile);
            $json_a = json_decode($string,true);
            //if (isset($json_a['uniquedeviceid'])) {
                $uniquedeviceid = $json_a['uniquedeviceid'];
            //}
            $jderror = $json_a['latesterror'];
        }

        $object->set("jdbuilddate", "$jdbuilddate");
        $object->set("jdversion", "$jdversion");
        $object->set("uniqueid", $uniquedeviceid);
        $object->set("jderror", $jderror);

        return $object->getAssoc();
    }

    public function setSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.jdownloader.setsettings');

        $object = $this->database->get($this->dataModelPath);
        $object->setAssoc($params);

        $downloadFolderRef = $object->get("download-sharedfolderref");
        $msg = "No directory for downloads set, please set one before enabling the service";

        if ($object->get("enable") === true && empty($downloadFolderRef)) {
            throw new \OMV\Exception($msg);
        }

        // Get current info as set in Jdownloader config file
        //$uniquedeviceid = "awaiting info";
        if (file_exists($this->jdownloaderSettingsFile)) {
            $string = file_get_contents($this->jdownloaderSettingsFile);
            $json_a = json_decode($string,true);
            //if (isset($json_a['uniquedeviceid'])) {
            //    $uniquedeviceid = $json_a['uniquedeviceid'];
            //}

            // Change valuse
            $json_a['email']     = $object->get("jdusername");
            $json_a['password']  = $object->get("jdpassword");

            file_put_contents($this->jdownloaderSettingsFile, json_encode($json_a));
        }

        if (file_exists($this->jdownloaderLocationFile)) {
            $string = file_get_contents($this->jdownloaderLocationFile);
        } else {
            $string = '{
  "defaultdownloadfolder" : "/home/jdownloader/Downloads"
}';
        }

        $json_b = json_decode($string,true);

        // Get the absolute shared folder path.
        if (!empty($object->get("download-sharedfolderref"))) {
            $db = \OMV\Config\Database::getInstance();
            $sfObject = $db->get("conf.system.sharedfolder", $object->get("download-sharedfolderref"));

            // Get the mount point configuration object to build the absolute shared folder path.
            $meObject = $db->get("conf.system.filesystem.mountpoint", $sfObject->get("mntentref"));

            // Return the absolute shared folder path.
            $jdpath = build_path(DIRECTORY_SEPARATOR, $meObject->get("dir"), $sfObject->get("reldirpath"));
        }

        if($object->get("download-dir") !== '') {
            if (substr($jdpath, -1) === '/') {
                $path = $jdpath . $object->get("download-dir");
            }else{
                $path = $jdpath . "/" . $object->get("download-dir");
            }
        }else{
            $path = $jdpath;
        }

        //Store path
        $json_b['defaultdownloadfolder'] = $path;

        file_put_contents($this->jdownloaderLocationFile, json_encode($json_b));
        if (!file_exists($json_b['defaultdownloadfolder'])) {
            mkdir($json_b['defaultdownloadfolder'], 0777, true);
        }

        if ($object->get('enablelog')) {
            file_put_contents($this->jdownloaderServiceFile, "[Unit]
Description=jdownloader
After=network.target

[Service]
User=jdownloader
Group=users
CPUQuota=20%
Type=simple
UMask=000
ExecStart=/bin/sh -c '/usr/bin/java -Djava.awt.headless=true -jar /opt/jdownloader/JDownloader.jar > " . $path . "/jdownloader.log 2>&1'
Restart=always

[Install]
WantedBy=multi-user.target");
            $cmd = "touch " . $path . "/jdownloader.log;chmod 777" . $path . "/jdownloader.log;systemctl daemon-reload";
            $this->exec($cmd);
        } else {
            file_put_contents($this->jdownloaderServiceFile, "[Unit]
Description=jdownloader
After=network.target

[Service]
User=jdownloader
Group=users
CPUQuota=20%
Type=simple
UMask=000
ExecStart=/usr/bin/java -Djava.awt.headless=true -jar /opt/jdownloader/JDownloader.jar
Restart=always
StandardOutput=null

[Install]
WantedBy=multi-user.target");
            $cmd = "systemctl daemon-reload";
            $this->exec($cmd);
        }

        //$object->set("jdbuilddate", "$jdbuilddate");
        //$object->set("jdversion", "$jdversion");
        //$object->set("uniqueid", $uniquedeviceid);

        $this->database->set($object);
        $this->dispatcher->notify(OMV_NOTIFY_MODIFY, $this->eventMessagePath, $object);

        return $object->getAssoc();
    }
}
