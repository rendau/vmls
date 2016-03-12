define([
    'tulip/ws', 'underscore', 'Backbone', 'tulip/nm'
], function(ws, _, BB, nm) {
    var idcnt = 4;
    var _M = BB.Model.extend({
        defaults: {
            'url': '',
            'name': '',
            'dsc': '',
        },
	validate: function(attrs, opts) {
	    if(!attrs.url) {
		nm.notify('URL не должен быть пустым', 'warn');
		return 'url';
	    }
	},
	sync: function(method, m, o) {
	    switch(method) {
	    case 'create':
		ws.send({
		    ns: 'camera', type: 'add', data: m.attributes
		}, function(reply) {
		    o.error(reply.type != 'success');
		    if(reply.type != 'success')
			nm.notify('Не удалось создать камеру: '+reply.data, 'warn');
		});
		break;
	    case 'update':
	    	ws.send({
	    	    ns: 'camera', type: 'change', data: m.attributes
	    	}, function(reply) {
	    	    o.error(reply.type != 'success');
	    	    if(reply.type != 'success')
	    		nm.notify('Не удалось изменить камеру: '+reply.data, 'warn');
	    	});
	    	break;
	    case 'delete':
	    	ws.send({
	    	    ns: 'camera', type: 'remove', id: m.id
	    	}, function(reply) {
	    	    o.error(reply.type != 'success');
	    	    if(reply.type != 'success')
	    		nm.notify('Не удалось удалить камеру: '+reply.data, 'warn');
	    	});
	    	break;
	    default:
		log('sync:', method);
		o.error();
	    }
	},
    });

    var cams = new BB.Collection([], {
	model: _M,
    });

    ws.addH('MSG', function(msg) {
        if(msg.ns != 'camera') return;
	switch(msg.type) {
	case 'added':
	    cams.add(msg.data);
	    break;
	case 'changed':
	    var attrs = msg.data;
	    if(attrs) {
		var cam = cams.get(attrs.id);
		cam && cam.set(attrs);
	    }
	    break;
	case 'removed':
	    var cam = cams.get(msg.id);
	    cam && cams.remove(cam);
	    break;
	}
    });

    return cams;

});
