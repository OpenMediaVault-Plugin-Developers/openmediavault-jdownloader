<?php

/**
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
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

require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/object.inc";
require_once "openmediavault/product.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/system.inc";
require_once "openmediavault/util.inc";

class OMVRpcServiceJdownloader extends OMVRpcServiceAbstract
{
    private $jdownloaderSettingsFile = "/opt/jdownloader/cfg/org.jdownloader.api.myjdownloader.MyJDownloaderSettings.json";

    private $jdownloaderLocationFile = "/opt/jdownloader/cfg/org.jdownloader.settings.GeneralSettings.json";

    private $jdownloaderBuildFile = "/opt/jdownloader/build.json";

    private function getPluginName()
    {
        return strtolower($this->getName());
    }

    private function getEventMessagePath()
    {
        return sprintf("org.openmediavault.service.%s", $this->getPluginName());
    }

    private function getXPath()
    {
        return sprintf("//services/%s", $this->getPluginName());
    }

    public function getName()
    {
        return "Jdownloader";
    }

    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
    }

    public function getSettings($params, $context)
    {
        global $xmlConfig;

        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        $jdbuilddate = "awaiting info";
        $jdversion   = "awaiting info";
        if (file_exists($this->jdownloaderBuildFile)) {
            $string       = file_get_contents($this->jdownloaderBuildFile);
            $json_c       = json_decode($string,true);
            $jdbuilddate  = $json_c['buildDate'];
            $jdversion    = $json_c['JDownloaderRevision'];
        }

        // Get current info as set in Jdownloader config file
        $uniquedeviceid = "awaiting info";
        $jderror = "awaiting info";
        if (file_exists($this->jdownloaderSettingsFile)) {
            $string = file_get_contents($this->jdownloaderSettingsFile);
            $json_a = json_decode($string,true);
            $uniquedeviceid = $json_a['uniquedeviceid'];
            $jderror = $json_a['latesterror'];
        }

        $object["enable"]                   = boolval($object["enable"]);
        $object["showtab"]                  = boolval($object["showtab"]);
        $object["jdbuilddate"]              = $jdbuilddate;
        $object["jdversion"]                = $jdversion;
        $object["uniquedeviceid"]           = $uniquedeviceid;
        $object["jderror"]                  = $jderror;
        $object["download.sharedfolderref"] = $object["download.sharedfolderref"];
        $object["download-dir"]             = $object["download-dir"];

        return $object;
    }

    public function setSettings($params, $context)
    {
        global $xmlConfig;

        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "enable"                      : { "type" : "boolean" },
                    "showtab"                     : { "type" : "boolean" },
                    "jdusername"                  : { "type" : "string" },
                    "jdpassword"                  : { "type" : "string" },
                    "download.sharedfolderref"    : {' . $GLOBALS["OMV_JSONSCHEMA_UUID_NONE"] . '},
                    "download-dir"                : { "type" : "string" }
                }
            }'
        );

        $downloadFolderRef = $params["download.sharedfolderref"];
        $msg = "No directory for downloads set, please set one before enabling the service";

        if ($params["enable"] === true && empty($downloadFolderRef)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_SAVE_FAILED, $msg);
        }

        // Get current info as set in Jdownloader config file
        $uniquedeviceid = "awaiting info";
        if (file_exists($this->jdownloaderSettingsFile)) {
            $string = file_get_contents($this->jdownloaderSettingsFile);
            $json_a = json_decode($string,true);
            $uniquedeviceid = $json_a['uniquedeviceid'];

            // Change valuse
            $json_a['email']     = $params["jdusername"];
            $json_a['password']  = $params["jdpassword"];

            file_put_contents($this->jdownloaderSettingsFile, json_encode($json_a));
        }

        if (file_exists($this->jdownloaderLocationFile)) {
            $string = file_get_contents($this->jdownloaderLocationFile);
            $json_b = json_decode($string,true);
            // Get the absolute shared folder path.
            $jdpath = OMVRpc::exec("ShareMgmt", "getPath", array(
                "uuid" => $params["download.sharedfolderref"]), $context);

            // Change valuse
            if($params["download-dir"] !== '') {
                $json_b['defaultdownloadfolder'] = $jdpath . "/" . $params["download-dir"];
            }else{
                $json_b['defaultdownloadfolder'] = $jdpath;
            }

            file_put_contents($this->jdownloaderLocationFile, json_encode($json_b));
            if (!file_exists($json_b['defaultdownloadfolder'])) {
                mkdir($json_b['defaultdownloadfolder'], 0777, true);
            }
        }


        $object = array(
            "enable"                     => array_boolval($params, "enable"),
            "showtab"                    => array_boolval($params, "showtab"),
            "jdusername"                 => $params["jdusername"],
            "jdpassword"                 => $params["jdpassword"],
            "uniqueid"                   => $uniquedeviceid,
            "download.sharedfolderref"   => $params["download.sharedfolderref"],
            "download-dir"               => $params["download-dir"]
        );

        if (false === $xmlConfig->replace($this->getXPath(), $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );
        return $object;
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceJdownloader());