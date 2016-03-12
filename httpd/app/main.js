require([
    'jquery', 'Backbone', 'tulip/ws', 'app', 'appv'
], function($, BB, ws, app, Appv) {

    var appv = new Appv();
    $('body').append(appv.$el);
    appv.render();

    ws.addH('CD', function(st) {
        st || location.reload();
    });

    ws.connect();

    // var Man = BB.Model.extend({
    //     defaults: {
    //         'name': 'DefaultName',
    //         'age': 18,
    //     },
    //     sync: function(method, obj, opts) {
    //         log('sync', method, obj);
    //         opts.success();
    //     },
    // });

    // var m1 = new Man();
    // m1.on('change', function(m, opts) {
    //     log('change', m.get('name'), opts);
    // });
    // m1.save({'name': 'newname'}, {
    //     wait: true,
    //     success: function(obj, reply, opts) {
    //         log('success:', obj, reply, opts);
    //     },
    //     error: function(obj, xhr, opts) {
    //         log('error:', obj, xhr, opts);
    //     },
    // });

    /*
    function onFrame(msg) {
        // log(msg);
    }

    function onLivestreamStart() {
        log('LIVE-STREAM STARTED!');
        con.addH('MSG', onFrame);
    }

    function authorized() {
        con.send({
            'ns': 'livestream', 'type': 'start',
            'cam_id': 1
        }, function(reply) {
            if(reply.type == 'success')
                onLivestreamStart();
            else
                console.error('live-stream start failed');
        });
    }

    function connected() {
        con.send({
            'ns': 'authorization', 'type': 'authorize',
            'login': 'admin', 'pass': 'admin',
        }, function(reply) {
            if(reply.type == 'success')
                authorized();
            else
                console.error('authorization failed');
        });
    }

    function disconnected() {
        con.removeH('MSG', onFrame);
        console.error('websocket disconnected');
    }

    con.connect();
    */

});
