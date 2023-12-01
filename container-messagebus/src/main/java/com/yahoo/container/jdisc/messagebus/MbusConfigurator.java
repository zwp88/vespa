package com.yahoo.container.jdisc.messagebus;

import com.yahoo.component.AbstractComponent;
import com.yahoo.component.annotation.Inject;
import com.yahoo.document.DocumentTypeManager;
import com.yahoo.documentapi.messagebus.protocol.DocumentProtocol;
import com.yahoo.messagebus.ConfigAgent;
import com.yahoo.messagebus.MessagebusConfig;

/**
 * This Exists only to update a session cache with the latest and greatest config, across container generations.
 * <p>
 * There is currently no way to force that this precedes other users of the SessionCache, without also forcing their
 * recreation when this is recreated, which is, again, the whole point of this component.
 * If we end up using an incomplete SessionCache, there will be trouble. We would need an additional mechanism in
 * the graph setup framework to solve this problem – something like {@code @Before/@After/@ImmediatelyAfter}.
 *
 * @author jonmv
 */
public class MbusConfigurator extends AbstractComponent {

    @Inject
    public MbusConfigurator(SessionCache cache, DocumentTypeManager manager, MessagebusConfig messagebusConfig) {
        cache.configure(new DocumentProtocol(manager), messagebusConfig);
    }

}
