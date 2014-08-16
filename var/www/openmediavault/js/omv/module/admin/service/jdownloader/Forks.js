/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2009-2013 Volker Theile
 * @copyright Copyright (c) 2013-2014 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/grid/Panel.js")
// require("js/omv/workspace/window/Form.js")
// require("js/omv/workspace/window/plugin/ConfigObject.js")
// require("js/omv/util/Format.js")
// require("js/omv/Rpc.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")

Ext.define("OMV.module.admin.service.couchpotato.Fork", {
    extend   : "OMV.workspace.window.Form",
    requires : [
        "OMV.workspace.window.plugin.ConfigObject"
    ],

    rpcService   : "Couchpotato",
    rpcGetMethod : "getFork",
    rpcSetMethod : "setFork",
    plugins      : [{
        ptype : "configobject"
    }],

    getFormItems : function() {
        return [{
            xtype      : "textfield",
            name       : "name",
            fieldLabel : _("Name"),
            allowBlank : false
        },{
            xtype      : "textfield",
            name       : "fork",
            fieldLabel : _("Fork"),
            allowBlank : false,
            plugins       : [{
                    ptype : "fieldinfo",
                    text  : _("Full fork URL (https://github.com/RuudBurger/CouchPotatoServer.git)")
                }]
        }];
    }
});

Ext.define("OMV.module.admin.service.couchpotato.Forks", {
    extend   : "OMV.workspace.grid.Panel",
    requires : [
        "OMV.Rpc",
        "OMV.data.Store",
        "OMV.data.Model",
        "OMV.data.proxy.Rpc",
        "OMV.util.Format"
    ],
    uses     : [
        "OMV.module.admin.service.couchpotato.Fork"
    ],

    hidePagingToolbar : false,
    stateful          : true,
    stateId           : "a982a76d-6804-4632-b31b-8b48c0ea6dde",
    columns           : [{
        text      : _("Name"),
        sortable  : true,
        dataIndex : "name",
        stateId   : "name"
    },{
        text      : _("Fork"),
        sortable  : true,
        dataIndex : "fork",
        stateId   : "fork"
    }],

    initComponent : function() {
        var me = this;
        Ext.apply(me, {
            store : Ext.create("OMV.data.Store", {
                autoLoad : true,
                model    : OMV.data.Model.createImplicit({
                    idProperty  : "uuid",
                    fields      : [
                        { name : "uuid", type : "string" },
                        { name : "name", type : "string" },
                        { name : "fork", type : "string" }
                    ]
                }),
                proxy : {
                    type    : "rpc",
                    rpcData : {
                        service : "Couchpotato",
                        method  : "getForks"
                    }
                }
            })
        });
        me.callParent(arguments);
    },

    onAddButton : function() {
        var me = this;
        Ext.create("OMV.module.admin.service.couchpotato.Fork", {
            title     : _("Add a git hub fork"),
            uuid      : OMV.UUID_UNDEFINED,
            listeners : {
                scope  : me,
                submit : function() {
                    this.doReload();
                    OMV.MessageBox.show({
                        title      : _("Confirmation"),
                        msg        : _("The information about available forks is out-of-date. You need to reload the information about available forks."),
                        buttons    : Ext.MessageBox.OKCANCEL,
                        buttonText : {
                            ok      : _("Reload"),
                            cancel  : _("Close")
                        },
                        fn         : function(answer) {
                            if("cancel" === answer)
                                return;
                            // Reload the page.
                            OMV.confirmPageUnload = false;
                            document.location.reload();
                        },
                        scope : me,
                        icon  : Ext.Msg.QUESTION
                    });
                }
            }
        }).show();
    },

    onEditButton : function() {
        var me = this;
        var record = me.getSelected();
        Ext.create("OMV.module.admin.service.couchpotato.Fork", {
            title     : _("Edit fork"),
            uuid      : record.get("uuid"),
            listeners : {
                scope  : me,
                submit : function() {
                    this.doReload();
                    OMV.MessageBox.show({
                        title      : _("Confirmation"),
                        msg        : _("The information about available forks is out-of-date. You need to reload the information about available forks."),
                        buttons    : Ext.MessageBox.OKCANCEL,
                        buttonText : {
                            ok      : _("Reload"),
                            cancel  : _("Close")
                        },
                        fn         : function(answer) {
                            if("cancel" === answer)
                                return;
                            // Reload the page.
                            OMV.confirmPageUnload = false;
                            document.location.reload();
                        },
                        scope : me,
                        icon  : Ext.Msg.QUESTION
                    });
                }
            }
        }).show();
    },

    doDeletion : function(record) {
        var me = this;
        OMV.Rpc.request({
            scope    : me,
            callback : me.onDeletion,
            rpcData  : {
                service : "Couchpotato",
                method  : "deleteFork",
                params  : {
                    uuid : record.get("uuid")
                }
            }
        });
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "scheduledjobs",
    path      : "/service/couchpotato",
    text      : _("Forks"),
    position  : 40,
    className : "OMV.module.admin.service.couchpotato.Forks"
});