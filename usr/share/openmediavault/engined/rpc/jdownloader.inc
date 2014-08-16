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

        $object["enable"]  = boolval($object["enable"]);
        $object["showtab"] = boolval($object["showtab"]);

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
                    "enable"      : { "type" : "boolean" },
                    "showtab"     : { "type" : "boolean" },
                    "jdusername"  : { "type" : "string" },
                    "jdpassword"  : { "type" : "string" }
                }
            }'
        );

        // Get existing configuration object
        $oldObject = $xmlConfig->get($this->getXPath());

        if (is_null($oldObject)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
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


        $object = array(
            "enable"      => array_boolval($params, "enable"),
            "showtab"     => array_boolval($params, "showtab"),
            "jdusername"  => $params["jdusername"],
            "jdpassword"  => $params["jdpassword"],
            "uniqueid"    => $uniquedeviceid,
            "jderror"     => $jderror
        );

        if (false === $xmlConfig->replace($this->getXPath(), $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        if ($oldObject["jdusername"] != $object["jdusername"] OR $oldObject["jdpassword"] != $object["jdpassword"]) {
            $cmd = sprintf("omv-mkconf %s userchange 2>&1", $this->getPluginName());
            if (0 !== $this->exec($cmd, $output)) {
                throw new OMVException(
                    OMVErrorMsg::E_EXEC_FAILED,
                    $cmd,
                    implode("\n", $output)
                );
            }
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