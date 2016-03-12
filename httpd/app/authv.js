define([
    'jquery', 'Backbone', 'crypto_sha1', 'tulip/ws',
    'app', 'tulip/dlg', 'tulip/nm', 'tulip/txt', 'tulip/btn'
], function($ , BB, crypto, ws, app, dlg, nm, Txt, Btn) {

    var V = BB.View.extend({
        tagName: 'div',
        className: 'auth',
        initialize: function(opts) {
	    this.txt = new Txt({
		width: '250px',
		psw: true,
		ph: 'пароль',
		keypress_h: this.txt_keypress_h.bind(this),
	    });
	    this.btn = new Btn({
		labelHtml: 'войти',
		click_h: this.submit.bind(this),
	    });
	    this.$el.append(this.txt.$dn);
	    // dlg.open({
	    // 	title: 'Авторизация',
	    // 	body: this.$el,
	    // 	foot: this.btn.$dn
	    // });
	    // this.txt.focus();
	    ws.addH('CD', function(st) {
		if(st) {
		    this.txt.val('admin');
		    this.submit();
		}
	    }, this);
        },
	txt_keypress_h: function(e) {
	    if(e.keyCode == 13)
		this.submit();
	},
	hide: function() {
	    dlg.close();
	},
	submit: function() {
	    var psw = this.txt.val();
	    if(!psw) return;
	    psw = crypto.SHA1(psw).toString(crypto.enc.Hex);
	    ws.send({
	    	'ns': 'authorization', 'type': 'authorize', 'pass': psw,
            }, function(reply) {
	    	if(reply.type == 'success') {
		    app.set('authorized', true);
		    dlg.close();
	    	} else if(reply.data == 'not_connected')
                    nm.notify('Ощибка: нет соединения', 'warn');
		else
                    nm.notify('Ошибка авторизации', 'warn');
            });
	}
    });

    return (new V);

});
